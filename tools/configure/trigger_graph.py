"""Typed hardware event plans, separate from cold-start dependency ordering.

The resolver accepts reviewed facts, not arbitrary authored hardware routes.
Execution cycles need explicit owned stop gates. This module never configures
hardware, creates workers or converts event edges into software callbacks.
"""

from dataclasses import asdict, dataclass

try:
    from .resource_graph import fail, identifier, json_value, record
except ImportError:
    from resource_graph import fail, identifier, json_value, record


@dataclass(frozen=True)
class TriggerNodeIR:
    id: str
    owner: str
    clock_domain: str
    events: tuple[str, ...]
    actions: tuple[str, ...]
    stop_actions: tuple[str, ...]


@dataclass(frozen=True)
class TriggerEdgeIR:
    source: str
    event: str
    target: str
    action: str
    route: str
    cross_domain: bool


@dataclass(frozen=True)
class AutonomousLoopIR:
    nodes: tuple[str, ...]
    stop_owner: str
    stop_node: str
    stop_action: str


@dataclass(frozen=True)
class TriggerPlanIR:
    nodes: tuple[TriggerNodeIR, ...]
    edges: tuple[TriggerEdgeIR, ...]
    startup_order: tuple[str, ...]
    loops: tuple[AutonomousLoopIR, ...]

    @classmethod
    def from_validated(cls, values):
        """Freeze validated event facts without inventing new selectors."""
        nodes = tuple(TriggerNodeIR(**{
            **item, "events": tuple(item["events"]),
            "actions": tuple(item["actions"]),
            "stop_actions": tuple(item["stop_actions"])})
                      for item in values["nodes"])
        loops = tuple(AutonomousLoopIR(**{
            **item, "nodes": tuple(item["nodes"])}) for item in values["loops"])
        edges = tuple(TriggerEdgeIR(**item) for item in values["edges"])
        return cls(nodes, edges,
                   tuple(values["startup_order"]), loops)

    def to_dict(self):
        return {"nodes": [json_value(asdict(item)) for item in self.nodes],
                "edges": [json_value(asdict(item)) for item in self.edges],
                "startup_order": list(self.startup_order),
                "loops": [json_value(asdict(item)) for item in self.loops]}


def _names(values, context):
    if not isinstance(values, (list, tuple)):
        fail(f"Invalid {context} sequence")
    result = tuple(identifier(value, context) for value in values)
    if len(set(result)) != len(result):
        fail(f"Duplicate {context}")
    return result


def _components(nodes, edges):
    """Find strongly connected components without recursion depth limits."""
    adjacency = {name: set() for name in nodes}
    reverse = {name: set() for name in nodes}
    for source, target in edges:
        adjacency[source].add(target)
        reverse[target].add(source)
    visited, order = set(), []
    for first in nodes:
        pending = [(first, False)]
        while pending:
            name, done = pending.pop()
            if done:
                order.append(name)
            elif name not in visited:
                visited.add(name)
                pending.append((name, True))
                pending.extend((target, False) for target in sorted(
                    adjacency[name], reverse=True) if target not in visited)
    visited, result = set(), []
    for first in reversed(order):
        if first in visited:
            continue
        pending, component = [first], []
        while pending:
            name = pending.pop()
            if name in visited:
                continue
            visited.add(name)
            component.append(name)
            pending.extend(reverse[name] - visited)
        if len(component) > 1 or first in adjacency[first]:
            result.append(frozenset(component))
    return result


def resolve(nodes, edges, *, startup=(), loops=()):
    """Return an immutable plan; startup edges are (prerequisite, dependent)."""
    selected = {}
    for value in nodes:
        record(value, {"id", "owner", "clock_domain", "events", "actions",
                       "stop_actions"}, set(), "trigger node")
        item = TriggerNodeIR(*(identifier(value[key], "trigger " + key)
                               for key in ("id", "owner", "clock_domain")),
                             _names(value["events"], "event"),
                             _names(value["actions"], "action"),
                             _names(value["stop_actions"], "stop action"))
        if not set(item.stop_actions).issubset(item.actions):
            fail("Trigger stop actions must be actual node actions")
        if item.id in selected:
            fail("Duplicate trigger node")
        selected[item.id] = item
    routes, links = set(), []
    for value in edges:
        record(value, {"source", "event", "target", "action", "route"},
               {"cross_domain"}, "trigger edge")
        cross = value.get("cross_domain", False)
        if type(cross) is not bool:
            fail("Trigger clock crossing requires a boolean route fact")
        item = TriggerEdgeIR(*(identifier(value[key], "trigger " + key)
                               for key in ("source", "event", "target",
                                           "action", "route")), cross)
        source, target = selected.get(item.source), selected.get(item.target)
        if (source is None or target is None or
                item.event not in source.events or
                item.action not in target.actions or item.route in routes):
            fail("Unknown or duplicated trigger route, node, event or action")
        if source.clock_domain != target.clock_domain and not cross:
            fail("Trigger crossing requires an explicit reviewed route fact")
        routes.add(item.route)
        links.append(item)
    prerequisites = {name: set() for name in selected}
    for dependency in startup:
        if (not isinstance(dependency, (tuple, list)) or len(dependency) != 2 or
                any(type(name) is not str or name not in selected
                    for name in dependency)):
            fail("Unknown startup dependency")
        before, after = dependency
        prerequisites[after].add(before)
    startup_order = []
    while prerequisites:
        ready = sorted(name for name, incoming in prerequisites.items()
                       if not incoming)
        if not ready:
            fail("Trigger startup dependency cycle")
        for name in ready:
            startup_order.append(name)
            del prerequisites[name]
        for incoming in prerequisites.values():
            incoming.difference_update(ready)
    event_edges = [(edge.source, edge.target) for edge in links]
    cyclic = _components(selected, event_edges)
    declarations = []
    for value in loops:
        record(value, {"nodes", "stop_owner", "stop_node", "stop_action"},
               set(), "autonomous loop")
        members = _names(value["nodes"], "autonomous loop node")
        controls = (identifier(value[key], "autonomous " + key)
                    for key in ("stop_owner", "stop_node", "stop_action"))
        item = AutonomousLoopIR(tuple(sorted(members)), *controls)
        node = selected.get(item.stop_node)
        component = frozenset(members)
        if (not members or component not in cyclic or node is None or
                item.stop_node not in component or
                node.owner != item.stop_owner or
                item.stop_action not in node.stop_actions or
                any(name not in selected or
                    selected[name].owner != item.stop_owner
                    for name in members) or
                any(frozenset(previous.nodes) == component
                    for previous in declarations)):
            fail("Invalid autonomous cycle or owned stop gate")
        remaining = {name: selected[name] for name in component
                     if name != item.stop_node}
        remaining_edges = [(source, target) for source, target in event_edges
                           if source in remaining and target in remaining]
        if _components(remaining, remaining_edges):
            fail("Autonomous stop gate does not interrupt every hardware cycle")
        declarations.append(item)
    if {frozenset(item.nodes) for item in declarations} != set(cyclic):
        fail("Hardware event cycle requires an explicit owned stop gate")
    return TriggerPlanIR(tuple(selected.values()), tuple(links),
                         tuple(startup_order), tuple(declarations))


def resolve_fixed(selected, routes):
    """Project only existing reviewed timer-triggered ADC modes.

No additional selector is invented. Their providers already validate the exact
timer and DMA route. Startup arms the DMA, then ADC, then the trigger; stop is
the reverse provider-owned drain contract. This is metadata, not a new emitter.
"""
    nodes, edges, startup = [], [], []
    for item in selected:
        if item["kind"] == "pwm_adc":
            owner = item["id"]
            trigger, adc, dma = (owner + suffix for suffix in
                                 (":trigger", ":adc", ":dma"))
            for name, clock, events, actions in (
                    (trigger, "controller:TIM1", ["oc4ref-rising"], ["stop"]),
                    (adc, "controller:ADC1", ["sample-ready"], ["sample", "stop"]),
                    (dma, "engine:DMA1", ["block-ready"], ["transfer", "stop"])):
                nodes.append({"id": name, "owner": owner, "clock_domain": clock,
                              "events": events, "actions": actions,
                              "stop_actions": ["stop"]})
            edges += [
                {"source": trigger, "event": "oc4ref-rising", "target": adc,
                 "action": "sample", "route": "TIM1:TRGO2:OC4REF:ADC1",
                 "cross_domain": True},
                {"source": adc, "event": "sample-ready", "target": dma,
                 "action": "transfer", "route": item["dma"][0],
                 "cross_domain": True}]
            startup += [(dma, adc), (adc, trigger)]
            continue
        if item["kind"] != "adc" or item["mode"] not in {
                "trigger-dma", "trigger-dma-continuous"}:
            continue
        route = routes[item["route"]]
        resources = route.get("mode_resources", {}).get(item["mode"], [])
        timers = [value for value in resources
                  if value.startswith("controller:")]
        if len(timers) != 1 or len(item.get("dma", ())) != 1:
            fail("Fixed ADC trigger requires one reviewed timer and DMA route")
        owner = item["id"]
        trigger, adc, dma = (owner + suffix
                             for suffix in (":trigger", ":adc", ":dma"))
        # These conservative clock identities do not assert a common clock
        # source or frequency. The reviewed hardware routes permit crossing.
        roles = ((trigger, timers[0], ["update"], ["stop"]),
                 (adc, "controller:" + item["controller"],
                  ["sample-ready"], ["sample", "stop"]),
                 (dma, "engine:" + item["dma"][0].split(":")[0],
                  ["block-ready"], ["transfer", "stop"]))
        for name, domain, events, actions in roles:
            nodes.append({"id": name, "owner": owner,
                          "clock_domain": domain,
                          "events": events, "actions": actions,
                          "stop_actions": ["stop"]})
        edges.extend([
            {"source": trigger, "event": "update", "target": adc,
             "action": "sample",
             "route": timers[0] + ":" + item["controller"],
             "cross_domain": True},
            {"source": adc, "event": "sample-ready", "target": dma,
             "action": "transfer", "route": item["dma"][0],
             "cross_domain": True}])
        startup.extend([(dma, adc), (adc, trigger)])
    return resolve(nodes, edges, startup=startup)

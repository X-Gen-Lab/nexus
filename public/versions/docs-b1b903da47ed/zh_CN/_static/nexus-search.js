/* Local Sphinx results retain their ranking, links and generated API symbols. */
(() => {
    "use strict";

    const installExclusionCompatibility = () => {
        if (typeof Search === "undefined"
                || typeof Search.performTermsSearch !== "function"
                || Search.performTermsSearch.nexusExclusionsCompatible) {
            return;
        }
        const native = Search.performTermsSearch;
        const compatible = (required, excluded) => {
            // Sphinx 7.4.7 treats scalar postings as arrays and stops all
            // candidates at the first exclusion. Keep its positive AND/score.
            const results = native.call(Search, required, new Set());
            if (!excluded || !excluded.size) {
                return results;
            }
            const index = Search._index;
            const excludedFiles = new Set();
            for (const term of excluded) {
                for (const table of [index.terms, index.titleterms]) {
                    const files = table[term];
                    if (Array.isArray(files)) {
                        files.forEach((file) => excludedFiles.add(file));
                    } else if (typeof files === "number") {
                        excludedFiles.add(files);
                    }
                }
            }
            const ids = new Map(index.docnames.map((name, file) => [name, file]));
            return results.filter((row) => !excludedFiles.has(ids.get(row[0])));
        };
        compatible.nexusExclusionsCompatible = true;
        Search.performTermsSearch = compatible;
    };

    const enhanceSearch = () => {
        const data = window.NEXUS_SEARCH_DATA;
        const output = document.getElementById("search-results");
        if (!output || !data || data.schema_version !== 1) {
            return;
        }
        const chinese = data.requested_language === "zh_CN";
        const labels = chinese ? {
            version: "版本", type: "类别", source: "原文", requested: "请求语言",
            actual: "实际语言", translation: "翻译", original: "原文",
            reviewed: "已审查", missing: "缺失，回退到原文",
            stale: "过期，回退到原文", tutorial: "教程", "how-to": "操作指南",
            reference: "参考", explanation: "架构说明", status: "状态记录"
        } : {
            version: "Version", type: "Type", source: "Source", requested: "Requested",
            actual: "Actual", translation: "Translation", original: "original",
            reviewed: "reviewed", missing: "missing, source fallback",
            stale: "stale, source fallback", tutorial: "tutorial", "how-to": "how-to",
            reference: "reference", explanation: "explanation", status: "status"
        };
        const names = {en: "English", zh_CN: "简体中文"};
        const query = new URLSearchParams(window.location.search).get("q") || "";
        const terms = query.toLowerCase().split(/\s+/u).filter(Boolean);
        const root = new URL(".", window.location.href);

        const recordFor = (link) => {
            try {
                const url = new URL(link.getAttribute("href"), document.baseURI);
                if (url.origin !== root.origin || !url.pathname.startsWith(root.pathname)) {
                    return null;
                }
                const key = decodeURIComponent(url.pathname.slice(root.pathname.length));
                if (key.split("/").some((part) => part === "." || part === "..")) {
                    return null;
                }
                return Object.hasOwn(data.records, key) ? data.records[key] : null;
            } catch (_error) {
                return null;
            }
        };

        const excerptFor = (text) => {
            const lower = text.toLowerCase();
            const matches = terms.map((term) => lower.indexOf(term)).filter((index) => index >= 0);
            const start = Math.max(0, (matches.length ? Math.min(...matches) : 0) - 80);
            const excerpt = text.slice(start, start + 280).trim();
            return (start ? "…" : "") + excerpt + (start + 280 < text.length ? "…" : "");
        };

        const enhanceItem = (item) => {
            const link = item.querySelector("a");
            const record = link && recordFor(link);
            if (!record) {
                return;
            }
            if (!item.querySelector(".nexus-search-identity")) {
                item.classList.add("nexus-search-result");
                item.dataset.nexusVersion = data.version;
                item.dataset.nexusType = record.type;
                item.dataset.nexusTranslation = record.status;
                item.dataset.nexusRequestedLanguage = record.requested_language;
                item.dataset.nexusActualLanguage = record.actual_language;
                link.title = record.title;
                const identity = document.createElement("p");
                identity.className = "nexus-search-identity";
                const fields = [
                    [labels.version, data.version],
                    [labels.type, labels[record.type] || record.type],
                    [labels.source, names[record.source_language]],
                    [labels.requested, names[record.requested_language]],
                    [labels.actual, names[record.actual_language]],
                    [labels.translation, labels[record.status] || record.status]
                ];
                fields.forEach(([label, value]) => {
                    const badge = document.createElement("span");
                    badge.className = "nexus-search-badge";
                    badge.textContent = label + ": " + value;
                    identity.appendChild(badge);
                });
                item.appendChild(identity);
                if (record.excerpt) {
                    const excerpt = document.createElement("p");
                    excerpt.className = "context nexus-search-excerpt";
                    excerpt.lang = record.actual_language.replace("_", "-");
                    excerpt.textContent = excerptFor(record.excerpt);
                    item.appendChild(excerpt);
                }
            }
            // Sphinx may append its asynchronously fetched summary after this item.
            // Keep the metadata-backed prose, excluding the page identity boilerplate.
            item.querySelectorAll("p.context:not(.nexus-search-excerpt)")
                .forEach((paragraph) => paragraph.remove());
        };

        const enhance = () => output.querySelectorAll("li").forEach(enhanceItem);
        new MutationObserver(enhance).observe(output, {childList: true, subtree: true});
        enhance();
    };

    const initializeSearch = () => {
        // This asset precedes searchtools.js, so install before Search.init's
        // DOMContentLoaded callback runs the first query from the URL.
        installExclusionCompatibility();
        enhanceSearch();
    };
    if (document.readyState === "loading") {
        document.addEventListener("DOMContentLoaded", initializeSearch, {once: true});
    } else {
        initializeSearch();
    }
})();

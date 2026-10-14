"""Read actual immutable final Git assets and verify documentation plus all proofs."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import subprocess
import sys
import tarfile
import time
from urllib.request import urlopen
import uuid


def parse(raw):
    def pairs(items):
        result = {}
        for key, value in items:
            if key in result:
                raise ValueError('Duplicate metadata key')
            result[key] = value
        return result
    def constant(_):
        raise ValueError('Nonfinite metadata constant')
    return json.loads(raw, object_pairs_hook=pairs, parse_constant=constant)


def relative(name):
    if not isinstance(name,str) or not name or '\\' in name or ':' in name or any(ord(c)<32 for c in name):
        raise ValueError('Invalid archive/asset path')
    path=PurePosixPath(name)
    if path.is_absolute() or '..' in path.parts or str(path)!=name:
        raise ValueError('Noncanonical archive/asset path')
    return name


def safe(path):
    path=Path(path).absolute()
    if any(p.is_symlink() for p in (path,*path.parents)):
        raise ValueError('Symlink path rejected')
    return path


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root',type=Path,required=True)
    parser.add_argument('--asset-commit',required=True)
    parser.add_argument('--asset-prefix',required=True)
    parser.add_argument('--tag',required=True)
    parser.add_argument('--evidence-root',type=Path,required=True)
    args=parser.parse_args()
    if not re.fullmatch('[0-9a-f]{40}',args.asset_commit) or not re.fullmatch('[A-Za-z0-9_.-]+',args.asset_prefix) or not re.fullmatch('[A-Za-z0-9_.-]+',args.tag):
        raise ValueError('Expected explicit immutable Git identity/namespace')
    root=safe(args.source_root);evidence=safe(args.evidence_root)
    if root.is_relative_to(evidence) or evidence.is_relative_to(root):raise ValueError('Evidence overlaps source')
    sys.path.insert(0,str(root))
    from tools.documentation import delivery
    from tools.evidence.identity import observe_git_source
    attempt=evidence/(datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ')+'-'+uuid.uuid4().hex)
    attempt.mkdir(parents=True)
    module=root/'tools/documentation/delivery.py';module_sha=hashlib.sha256(module.read_bytes()).hexdigest()
    receipt={'schema_version':1,'kind':'independent_final_documentation_retention_verification',
        'status':'running','started_at':datetime.now(timezone.utc).isoformat(),
        'command':[sys.executable,*sys.argv],'script_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        'asset_commit':args.asset_commit,'asset_prefix':args.asset_prefix,'tag':args.tag,
        'source_before':observe_git_source(root),'production_module_sha256':module_sha,
        'physical_status':'not_executed','browser_execution':'not_executed','commands':[],'requests':[]}
    record=attempt/'execution.json'
    def save():record.write_text(json.dumps(receipt,indent=2)+'\n')
    save();started=time.monotonic()
    def api(path,label):
        command=['gh','api','repos/X-Gen-Lab/nexus/'+path]
        response=subprocess.run(command,capture_output=True,timeout=60)
        (attempt/(label+'.json')).write_bytes(response.stdout);(attempt/(label+'.stderr')).write_bytes(response.stderr)
        receipt['commands'].append({'argv':command,'returncode':response.returncode,'stdout_sha256':hashlib.sha256(response.stdout).hexdigest()})
        if response.returncode:raise ValueError('Actual remote API request failed: '+label)
        return parse(response.stdout)
    base='https://raw.githubusercontent.com/X-Gen-Lab/nexus/'+args.asset_commit+'/'
    def fetch(name,expected):
        relative(name);target=safe(attempt/'downloaded'/name);target.parent.mkdir(parents=True,exist_ok=True)
        count=0;sha=hashlib.sha256();blob=hashlib.sha1(('blob '+str(expected['bytes'])+'\0').encode())
        with urlopen(base+name,timeout=60) as response,target.open('xb') as output:
            if response.status!=200 or response.geturl()!=base+name:raise ValueError('HTTP status/final URL differs')
            while block:=response.read(1024*1024):
                count+=len(block);sha.update(block);blob.update(block);output.write(block)
                if count>expected['bytes'] or time.monotonic()-started>300:raise ValueError('HTTP bytes/time exceeds bound')
        if count!=expected['bytes'] or blob.hexdigest()!=expected['blob'] or ('sha256' in expected and sha.hexdigest()!=expected['sha256']):
            raise ValueError('Actual remote bytes/SHA identity differs: '+name)
        receipt['requests'].append({'url':base+name,'final_url':base+name,'status':200,'bytes':count,
            'sha256':sha.hexdigest(),'git_blob_sha1':blob.hexdigest(),'path':str(target)})
        save();return target
    try:
        if receipt['source_before']['dirty'] or receipt['source_before']['commit']!='5a7a75c321ea173df8692714f95a8df13715293d':raise ValueError('Expected clean final Source5 observer')
        commit=api('git/commits/'+args.asset_commit,'remote-commit')
        if commit['sha']!=args.asset_commit:raise ValueError('Remote commit differs')
        tree=api('git/trees/'+commit['tree']['sha']+'?recursive=1','remote-tree')
        if tree.get('truncated'):raise ValueError('Remote inventory truncated')
        blobs={row['path']:row for row in tree['tree'] if row['type']=='blob'}
        tag=api('git/ref/tags/'+args.tag,'remote-tag')['object']
        for index in range(5):
            if tag['type']=='commit':break
            if tag['type']!='tag':raise ValueError('Unexpected Git tag object')
            tag=api('git/tags/'+tag['sha'],'remote-tag-object-'+str(index))['object']
        if tag['type']!='commit' or tag['sha']!=args.asset_commit:raise ValueError('Final immutable tag binding differs')
        row=blobs['assets.json'];inventory_path=fetch('assets.json',{'bytes':row['size'],'blob':row['sha']})
        inventory=parse(inventory_path.read_bytes());assets={}
        for item in inventory['members']:
            name=relative(item['path'])
            if name in assets or type(item['bytes']) is not int or item['bytes']<0 or not isinstance(item['sha256'],str) or not re.fullmatch('[0-9a-f]{64}',item['sha256']):raise ValueError('Invalid asset inventory')
            assets[name]=item
        prefix=args.asset_prefix+'/'
        wanted={name for name in assets if name.startswith(prefix)}
        actual={name for name in blobs if name.startswith(prefix)}
        expected_names={'README.txt','documentation-package.json','docs-20261014-5a7a75c321ea.tar.gz','final-journal-evidence.json','final-journal-evidence.tar.gz'}
        if wanted!=actual or {name.removeprefix(prefix) for name in wanted}!=expected_names:raise ValueError('Final exact five-asset closure differs')
        paths={}
        for name in sorted(wanted):
            if blobs[name]['size']!=assets[name]['bytes']:raise ValueError('Git tree bytes differ from asset inventory')
            paths[name.removeprefix(prefix)]=fetch(name,{**assets[name],'blob':blobs[name]['sha']})
        package=parse(paths['documentation-package.json'].read_bytes())
        if package['archive_sha256']!='79740121a640fa8e829f8110fb3e0fc5900474e7daa6067b5faf7a99f05ae6e6' or package['manifest_sha256']!='37db6ca0218ce0f70def6baf33c3c23a370503ef61ee58073c8f3a84516adf34':raise ValueError('Final documentation external anchors differ')
        archive=paths['docs-20261014-5a7a75c321ea.tar.gz']
        if archive.stat().st_size!=7099794 or hashlib.file_digest(archive.open('rb'),'sha256').hexdigest()!=package['archive_sha256']:raise ValueError('Final remote documentation archive differs')
        manifest=delivery.verify(archive,expected_manifest_sha256=package['manifest_sha256'])
        if len(manifest['files'])!=1600 or manifest['documentation_source']['commit']!='5a7a75c321ea173df8692714f95a8df13715293d':raise ValueError('Final raw Git archive Source5/count differs')
        supplement=parse(paths['final-journal-evidence.json'].read_bytes())
        if supplement['schema_version']!=1 or supplement['kind']!='nexus_final_documentation_journal_evidence' or supplement['status']!='passed' or supplement['documentation_commit']!=manifest['documentation_source']['commit'] or supplement['physical_hil']!='not_executed':raise ValueError('Supplement source/state boundary differs')
        expected={}
        for item in supplement['members']:
            name=relative(item['path'])
            if name in expected or set(item)!={'path','bytes','sha256','original_path'} or type(item['bytes']) is not int or item['bytes']<0 or not isinstance(item['sha256'],str) or not re.fullmatch('[0-9a-f]{64}',item['sha256']):raise ValueError('Invalid/duplicate supplement member')
            expected[name]={key:item[key] for key in ('bytes','sha256')}
        if len(expected)!=119 or any(str(parent) in expected for name in expected for parent in PurePosixPath(name).parents):raise ValueError('Supplement count or parent/file collision differs')
        proof=paths['final-journal-evidence.tar.gz'];proof_sha=hashlib.file_digest(proof.open('rb'),'sha256').hexdigest()
        if proof.stat().st_size!=28531727 or proof_sha!='4670eab7e605be9fab49960cd85ec41a1a0aec7e91716093b4fadc55ef0686d0' or supplement['archive']!={'path':'final-journal-evidence.tar.gz','bytes':28531727,'sha256':proof_sha}:raise ValueError('Supplement archive external anchors differ')
        observations=[];seen=set()
        with tarfile.open(proof,'r|gz') as tar:
            for item in tar:
                name=relative(item.name)
                if name in seen or not item.isfile() or name not in expected or item.size!=expected[name]['bytes']:raise ValueError('Extra/linked/duplicate/missing supplement closure')
                seen.add(name);stream=tar.extractfile(item);sha=hashlib.file_digest(stream,'sha256').hexdigest()
                if sha!=expected[name]['sha256']:raise ValueError('Supplement member modified: '+name)
                observations.append({'path':name,'bytes':item.size,'sha256':sha})
        if seen!=set(expected) or hashlib.file_digest(proof.open('rb'),'sha256').hexdigest()!=proof_sha:raise ValueError('Supplement missing or changed member')
        observation_file=attempt/'supplement-member-observations.json';observation_file.write_text(json.dumps({'schema_version':1,'status':'passed','members':observations},indent=2)+'\n')
        if hashlib.sha256(module.read_bytes()).hexdigest()!=module_sha:raise ValueError('Production verifier changed')
        receipt.update(status='passed',returncode=0,verified_assets=5,verified_documentation_files=1600,
            documentation_source=manifest['documentation_source'],documentation_archive_bytes=archive.stat().st_size,
            documentation_archive_sha256=package['archive_sha256'],documentation_manifest_sha256=package['manifest_sha256'],
            supplement_archive_bytes=proof.stat().st_size,supplement_archive_sha256=proof_sha,verified_supplement_members=119,
            supplement_observations={'path':str(observation_file),'bytes':observation_file.stat().st_size,'sha256':hashlib.sha256(observation_file.read_bytes()).hexdigest()},
            limitations=['No tools installation or new browser/physical HIL execution.','Documentation package verified from actual raw Git delivery; independent Pages download/restore was separately executed.'])
        code=0
    except Exception as error:
        receipt.update(status='failed',returncode=1,error=str(error));code=1
    finally:
        receipt.update(elapsed_seconds=round(time.monotonic()-started,3),finished_at=datetime.now(timezone.utc).isoformat(),source_after=observe_git_source(root))
        if receipt['source_before']!=receipt['source_after'] or receipt['source_after']['dirty']:
            receipt.update(status='failed',returncode=1,error='Final clean observer source changed');code=1
        save()
    print(json.dumps({'status':receipt['status'],'receipt':str(record),'elapsed_seconds':receipt['elapsed_seconds']}),flush=True)
    return code

if __name__=='__main__':
    raise SystemExit(main())

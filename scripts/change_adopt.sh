#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/harness_lock.sh"
switch=0
[[ $# -ge 1 && $# -le 2 ]] || { echo "usage: $0 <kebab-name> [--switch]" >&2; exit 2; }
change=$1; harness_validate_change_id "$change"
if [[ $# -eq 2 ]]; then [[ "$2" == --switch ]] || { echo "[ERR] unknown option: $2" >&2; exit 2; }; switch=1; fi
harness_lock_acquire change-adopt "$change"; harness_require_no_archive_failure
harness_assert_change_dir "$change" || { echo "[ERR] change is missing, archived, or unsafe: $change" >&2; exit 4; }
active=$(harness_active_optional)
[[ -z "$active" || "$active" == "$change" ]] || harness_require_isolation_authority "$active" change-adopt
[[ -z "$active" || "$active" == "$change" || "$switch" -eq 1 ]] || { echo "[ERR] '$active' is active; pass --switch explicitly" >&2; exit 4; }
harness_path="openspec/changes/$change/harness"
[[ ! -e "$harness_path" && ! -L "$harness_path" ]] || { echo "[ERR] change already has harness/evidence; refusing to overwrite: $change" >&2; exit 4; }
scripts/openspec_preflight.sh >/dev/null || { echo '[ERR] fixed OpenSpec preflight failed' >&2; exit 6; }
status_json=$(mktemp "${TMPDIR:-/tmp}/autoai-adopt-status.XXXXXX")
trap 'rm -f -- "$status_json"; harness_lock_release' EXIT
scripts/openspec_cli.sh status --change "$change" --json > "$status_json" || { echo '[ERR] OpenSpec status failed; no evidence was created' >&2; exit 6; }
node - "$status_json" "$change" <<'NODE' || { echo '[ERR] existing change failed the OpenSpec 1.6 status/directory safety contract; no evidence was created' >&2; exit 6; }
const fs=require('fs'),path=require('path');
const [statusFile,change]=process.argv.slice(2),root=fs.realpathSync('.'),target=path.join(root,'openspec','changes',change),fail=m=>{throw Error(m)};
const safeFile=p=>{const s=fs.lstatSync(p);if(!s.isFile()||s.isSymbolicLink())fail('unsafe file: '+p)};
const walk=p=>{for(const name of fs.readdirSync(p)){const child=path.join(p,name),s=fs.lstatSync(child);if(s.isSymbolicLink()||!s.isFile()&&!s.isDirectory())fail('unsafe change entry: '+child);if(s.isDirectory())walk(child)}};
const st=fs.lstatSync(target);if(!st.isDirectory()||st.isSymbolicLink())fail('unsafe change directory');
if(fs.existsSync(path.join(target,'harness')))fail('harness already exists');
walk(target);safeFile(path.join(target,'.openspec.yaml'));
const schemaLines=fs.readFileSync(path.join(target,'.openspec.yaml'),'utf8').split(/\r?\n/).map(x=>x.replace(/\s+#.*$/,'').trim()).filter(x=>x&&!x.startsWith('#')&&/^schema\s*:/.test(x));
if(schemaLines.length!==1||schemaLines[0].replace(/^schema\s*:\s*/,'').replace(/^(['"])(.*)\1$/,'$2').trim()!=='spec-driven')fail('metadata schema mismatch');
const v=JSON.parse(fs.readFileSync(statusFile,'utf8')),required=['proposal','design','specs','tasks'],outputs={proposal:'proposal.md',design:'design.md',specs:'specs/**/*.md',tasks:'tasks.md'};
if(v?.changeName!==change||v?.schemaName!=='spec-driven'||typeof v.isComplete!=='boolean'||!path.isAbsolute(v.changeRoot||'')||path.resolve(v.changeRoot)!==target)fail('status identity mismatch');
if(v?.root?.path!==root||typeof v.root.source!=='string'||!v.root.source||v?.planningHome?.root!==root||v.planningHome.changesDir!==path.join(root,'openspec','changes')||v.planningHome.defaultSchema!=='spec-driven')fail('status root mismatch');
if(!v.artifactPaths||typeof v.artifactPaths!=='object'||Array.isArray(v.artifactPaths)||Object.keys(v.artifactPaths).sort().join(',')!==required.slice().sort().join(','))fail('artifactPaths mismatch');
for(const id of required){const x=v.artifactPaths[id],expected=outputs[id],resolved=path.join(target,expected);if(!x||x.outputPath!==expected||!path.isAbsolute(x.resolvedOutputPath||'')||path.resolve(x.resolvedOutputPath)!==path.resolve(resolved)||!Array.isArray(x.existingOutputPaths))fail('artifact path contract mismatch: '+id);for(const p of x.existingOutputPaths){if(typeof p!=='string'||!path.isAbsolute(p))fail('non-absolute existing artifact');const r=path.resolve(p);if(r!==target&&!r.startsWith(target+path.sep))fail('artifact escaped change');safeFile(r);if(id==='specs'){if(!r.startsWith(path.join(target,'specs')+path.sep)||!r.endsWith('.md'))fail('invalid spec artifact')}else if(r!==path.join(target,expected))fail('unexpected artifact path')}}
if(!Array.isArray(v.artifacts)||v.artifacts.length!==required.length)fail('artifact status count mismatch');const seen=new Set;for(const x of v.artifacts){if(!x||!required.includes(x.id)||seen.has(x.id)||!['blocked','ready','done'].includes(x.status)||x.outputPath!==outputs[x.id])fail('artifact status mismatch');seen.add(x.id)}if(seen.size!==required.length||v.isComplete!==v.artifacts.every(x=>x.status==='done'))fail('status completion mismatch');
NODE
node - "$change" <<'NODE' || { echo '[ERR] could not atomically install initial evidence; no existing harness was replaced' >&2; exit 6; }
const fs=require('fs'),path=require('path'),change=process.argv[2],base=path.join('openspec','changes',change),h=path.join(base,'harness');
if(fs.existsSync(h))throw Error('harness already exists');
for(const name of fs.readdirSync(base)){const p=path.join(base,name),s=fs.lstatSync(p);if(s.isSymbolicLink()||!s.isFile()&&!s.isDirectory())throw Error('unsafe change entry: '+p)}
const files={
  'ai_snapshot.json':JSON.stringify({schema_version:4,phase:'planning',planned_base_specs_fingerprint:null,planned_change_fingerprint:null,planned_tdd_policy_sha256:null,planned_integration_completeness_sha256:null,planning_approved_at:null,implementation_base_commit:null,adopted_preexisting_paths:[],implementation_baselined_at:null,current_step:'review adopted change artifacts',next_step:'strict validate, run integration plan check, and obtain human review'},null,2)+'\n',
  'verification.json':JSON.stringify({schema_version:3,change_name:change,migration:null,tasks:[]},null,2)+'\n',
  'verification.md':`# Verification — ${change}\n\n`,
  'evaluation.md':`# Evaluation history — ${change}\n\n`,
  'defect-rca.md':`# Defect RCA — ${change}\n\n`
};
let staging;try{staging=fs.mkdtempSync(path.join(base,'.harness-adopt-'));fs.chmodSync(staging,0o755);for(const [name,content] of Object.entries(files))fs.writeFileSync(path.join(staging,name),content,{encoding:'utf8',mode:0o644,flag:'wx'});if(fs.existsSync(h))throw Error('harness appeared concurrently');fs.renameSync(staging,h);staging=null}catch(e){if(staging)fs.rmSync(staging,{recursive:true,force:true});throw e}
NODE
harness_lock_bind_change "$change"
if [[ -z "$active" || "$active" == "$change" || "$switch" -eq 1 ]]; then
    harness_atomic_json_update ai_snapshot.json active_change="$change" phase=planning current_step=change-adopted next_step="Review existing OpenSpec artifacts and obtain approval"
fi
echo "Adopted existing change without replacing artifacts or evidence: $change"

#!/usr/bin/env bash
set -euo pipefail
kind=all; change=; as_json=0
while [[ $# -gt 0 ]]; do case "$1" in --kind) kind=${2:?}; shift ;; --change) change=${2:?}; shift ;; --json) as_json=1 ;; *) exit 2 ;; esac; shift; done
case "$kind" in source|profile|artifact|base-specs|planning|all) ;; *) exit 2 ;; esac
if [[ "$kind" == artifact || "$kind" == planning || "$kind" == all ]]; then [[ -n "$change" ]] || change=$(node -p "JSON.parse(require('fs').readFileSync('ai_snapshot.json')).active_change||''"); [[ "$change" =~ ^[a-z][a-z0-9]*(-[a-z0-9]+)*$ && "$change" != archive && "$change" != archive-* && "$change" != stale && "$change" != stale-* ]] || exit 2; fi
node - "$kind" "$change" "$as_json" <<'NODE'
const fs=require('fs'),path=require('path'),crypto=require('crypto'),cp=require('child_process'),{TextDecoder}=require('util');const [kind,change,asJson]=process.argv.slice(2),root=process.cwd(),utf8=new TextDecoder('utf-8',{fatal:true});const sha=b=>'sha256:'+crypto.createHash('sha256').update(b).digest('hex'),nul=b=>utf8.decode(b).split('\0').filter(Boolean);
function rec(rel,indexMode=''){
  let st;try{st=fs.lstatSync(rel)}catch(e){if(e.code==='ENOENT'){if(indexMode==='160000')throw Error(`uninitialized gitlink: ${rel}`);return `${rel}\x00${indexMode||'000000'}\x00deleted\x00<deleted>\x00`}throw e}
  if(st.isSymbolicLink()){
    if(!indexMode||controlled(rel))throw Error(`symbolic link blocked: ${rel}`);
    const target=fs.readlinkSync(rel,'buffer');
    return `${rel}\x00120000\x00symlink\x00${sha(Buffer.concat([Buffer.from('120000\0'),target]))}\x00`
  }
  if(st.isDirectory()){
    if(indexMode!=='160000')throw Error(`unexpected directory: ${rel}`);
    const row=cp.execFileSync('git',['ls-files','-s','--',rel],{encoding:'utf8'}).trim(),oid=row.split(/\s+/)[1]||'';let head,status,diffHash,untrackedHash;
    try{
      const top=cp.execFileSync('git',['-C',rel,'rev-parse','--show-toplevel'],{encoding:'utf8'}).trim();
      if(!top||fs.realpathSync(top)!==fs.realpathSync(rel))throw Error('gitlink checkout is not an independent worktree');
      head=cp.execFileSync('git',['-C',rel,'rev-parse','HEAD'],{encoding:'utf8'}).trim();
      status=cp.execFileSync('git',['-C',rel,'status','--porcelain=v1','-z']).toString('hex');
      diffHash=sha(cp.execFileSync('git',['-C',rel,'diff','--no-ext-diff','--no-textconv','--binary','HEAD','--']));
      const records=[];for(const p of nul(cp.execFileSync('git',['-C',rel,'ls-files','--others','--exclude-standard','-z']))){const full=path.join(rel,p),s=fs.lstatSync(full);if(s.isSymbolicLink()||!s.isFile())throw Error(`unsupported untracked gitlink entry: ${rel}/${p}`);records.push(`${p}\x00${(s.mode&0o111)?'100755':'100644'}\x00${sha(fs.readFileSync(full))}\x00`)}records.sort((a,b)=>Buffer.from(a).compare(Buffer.from(b)));untrackedHash=sha(Buffer.from(records.length?records.join(''):'<empty>\0'));
    }catch{throw Error(`unstable gitlink: ${rel}`)}
    return `${rel}\x00160000\x00gitlink\x00${oid}\x00${head}\x00${status}\x00${diffHash}\x00${untrackedHash}\x00`
  }
  if(!st.isFile())throw Error(`unsupported type: ${rel}`);const workMode=(st.mode&0o111)?'100755':'100644';return `${rel}\x00${workMode}\x00file\x00${sha(fs.readFileSync(rel))}\x00`
}
const digest=a=>{a.sort((x,y)=>Buffer.from(x).compare(Buffer.from(y)));return sha(Buffer.from(a.length?a.join(''):'<empty>\0'))};
let managedPaths;
function controlled(rel){
  if(rel==='ai_snapshot.json'||rel==='claude-progress.txt'||rel==='session-state.md'||
      rel==='AGENTS.md'||rel==='CLAUDE.md'||rel==='PROJECT_ATTRIBUTION.md'||
      rel==='openspec'||rel.startsWith('openspec/')||rel==='.ai-harness'||rel.startsWith('.ai-harness/'))return true;
  if(managedPaths===undefined){
    managedPaths=new Set;
    try{
      const manifestPath='.ai-harness/manifest.json',st=fs.lstatSync(manifestPath);
      if(!st.isFile()||st.isSymbolicLink())throw Error('managed manifest is unsafe');
      const manifest=JSON.parse(fs.readFileSync(manifestPath,'utf8'));
      if(!Array.isArray(manifest.managed_paths))throw Error('managed manifest paths are invalid');
      for(const row of manifest.managed_paths)if(row&&typeof row.path==='string')managedPaths.add(row.path);
    }catch(e){
      if(e.code!=='ENOENT')throw e;
    }
  }
  return managedPaths.has(rel);
}
function source(){
  const files=nul(cp.execFileSync('git',['ls-files','-co','--exclude-standard','-z'])),idx=new Map;
  for(const row of nul(cp.execFileSync('git',['ls-files','-s','-z']))){const m=row.match(/^(\d+) ([0-9a-f]+) \d+\t([\s\S]+)$/);if(m)idx.set(m[3],m[1])}
  const runtime=p=>p==='ai_snapshot.json'||p==='claude-progress.txt'||p==='session-state.md'||p==='.ai-harness/archive-transaction.json'||p==='openspec'||p.startsWith('openspec/')||p.startsWith('.ai-harness/locks/')||p.startsWith('.ai-harness/logs/')||p.startsWith('.ai-harness/migrations/')||p.startsWith('.ai-harness/derived/');
  let generated=[];
  try{
    const lib=require(process.cwd()+'/scripts/project_profile_lib.js'),loaded=lib.parseProfile(process.cwd(),process.cwd()+'/.ai-harness/project-profile.json');
    generated=[...loaded.modules.values()].flatMap(module=>module.path_roles.generated);
  }catch{}
  const glob=value=>{let s='^';for(let i=0;i<value.length;i++){const c=value[i];if(c==='*'&&value[i+1]==='*'){const segmentStart=i===0||value[i-1]==='/';if(segmentStart&&value[i+2]==='/'){s+='(?:.*/)?';i+=2}else if(segmentStart&&i+2===value.length){s+='.*';i++}else{s+='[^/]*';i++}}else if(c==='*')s+='[^/]*';else if(c==='?')s+='[^/]';else s+='\\.^$+{}()|[]'.includes(c)?'\\'+c:c}return new RegExp(s+'$')},generatedRegex=generated.map(glob);
  const ex=p=>runtime(p)||!idx.has(p)&&generatedRegex.some(re=>re.test(p));
  return digest([...new Set(files)].filter(p=>!ex(p)).map(p=>rec(p,idx.get(p)||'')));
}
function profile(){
  const lib=require(process.cwd()+'/scripts/project_profile_lib.js'),loaded=lib.parseProfile(process.cwd(),process.cwd()+'/.ai-harness/project-profile.json');
  return loaded.profile_sha256;
}
function walk(dir,accept,out=[],prune=()=>false){if(!fs.existsSync(dir))return out;const st=fs.lstatSync(dir),rel=path.relative(root,dir).split(path.sep).join('/');if(st.isSymbolicLink())throw Error(`symbolic link blocked: ${rel}`);if(st.isDirectory()){if(prune(rel))return out;for(const n of fs.readdirSync(dir).sort())walk(path.join(dir,n),accept,out,prune)}else if(st.isFile()&&accept(rel))out.push(rec(rel));return out}
function artifact(){const b=`openspec/changes/${change}`,exact=new Set([`${b}/.openspec.yaml`,`${b}/proposal.md`,`${b}/design.md`,`${b}/tasks.md`]);return digest(walk(b,p=>exact.has(p)||p.startsWith(`${b}/specs/`)&&p.endsWith('.md'),[],p=>p===`${b}/harness`||p.startsWith(`${b}/harness/`)))}
function base(){return digest(walk('openspec/specs',p=>p.startsWith('openspec/specs/')&&p.endsWith('.md')))}
try{const r={};if(kind==='source'||kind==='all')r.source_fingerprint=source();if(kind==='profile'||kind==='all')r.profile_sha256=profile();if(kind==='artifact'||kind==='all')r.artifact_fingerprint=artifact();if(kind==='base-specs'||kind==='all')r.base_specs_fingerprint=base();if(kind==='planning'){const p=require(process.cwd()+'/scripts/manifest_policy.js').planningState(process.cwd(),change);r.planning_fingerprint=p.planning_fingerprint;r.tdd_policy_sha256=p.tdd_policy_sha256;if(p.integration_completeness_sha256)r.integration_completeness_sha256=p.integration_completeness_sha256}process.stdout.write(asJson==='1'||kind==='all'?JSON.stringify(r,null,2)+'\n':Object.values(r)[0]+'\n')}catch(e){console.error('[ERR] '+e.message);process.exit(6)}
NODE

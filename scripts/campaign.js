#!/usr/bin/env node
'use strict';
const fs=require('fs'),path=require('path'),crypto=require('crypto'),cp=require('child_process'),lib=require('./project_profile_lib.js');
const args=process.argv.slice(2);if(args.length<2){console.error('usage: campaign.sh <campaign-id> --check|--status|--next-ready [--json]');process.exit(2)}
const id=args.shift(),action=args.shift(),json=args.includes('--json');if(!/^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$/.test(id)||!['--check','--status','--next-ready'].includes(action)||args.some(x=>x!=='--json')){console.error('[ERR] invalid campaign arguments');process.exit(2)}
const root=lib.ensureRepositoryRoot(process.cwd()),file=path.join(root,'.ai-harness','campaigns',id+'.json'),sha=v=>'sha256:'+crypto.createHash('sha256').update(v).digest('hex');
const closed=(o,keys,label)=>{if(!o||typeof o!=='object'||Array.isArray(o)||Object.keys(o).some(k=>!keys.includes(k))||keys.some(k=>!Object.prototype.hasOwnProperty.call(o,k)))throw Error(label+' schema mismatch')};
const changeId=value=>typeof value==='string'&&/^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$/.test(value)&&value!=='archive'&&!value.startsWith('archive-')&&value!=='stale'&&!value.startsWith('stale-');
const digestPattern=/^sha256:[0-9a-f]{64}$/,receiptKeys=['schema_version','change_name','archived_as','archived_at','profile_sha256','source_fingerprint','artifact_fingerprint','base_specs_fingerprint_before','archive_output_sha256','main_specs_validation_sha256','authority'];
function archivedArtifactFingerprint(relative,change){
  const records=[],base=`openspec/changes/${change}`,add=(actualRelative,logicalRelative,label)=>{
    const entry=lib.safeRepositoryEntry(root,actualRelative,label,'file'),mode=(entry.stat.mode&0o111)?'100755':'100644';
    records.push(`${logicalRelative}\x00${mode}\x00file\x00${sha(fs.readFileSync(entry.path))}\x00`);
  };
  for(const name of ['.openspec.yaml','proposal.md','design.md','tasks.md'])add(`${relative}/${name}`,`${base}/${name}`,'archived Campaign artifact');
  const specsRelative=relative+'/specs',specs=lib.safeRepositoryEntry(root,specsRelative,'archived Campaign specs','directory').path,specFiles=[];
  const walk=(directory,logical)=>{
    for(const name of fs.readdirSync(directory).sort((a,b)=>Buffer.from(a).compare(Buffer.from(b)))){
      const full=path.join(directory,name),st=fs.lstatSync(full),childLogical=logical+'/'+name;
      if(st.isSymbolicLink())throw Error('archived Campaign specs contain a symbolic link: '+childLogical);
      if(st.isDirectory())walk(full,childLogical);
      else if(st.isFile()&&name.endsWith('.md'))specFiles.push({full,logical:childLogical,stat:st});
      else if(!st.isFile())throw Error('archived Campaign specs contain an unsupported entry: '+childLogical);
    }
  };
  walk(specs,base+'/specs');
  if(!specFiles.some(row=>row.logical.endsWith('/spec.md')))throw Error('archived Campaign change has no delta spec: '+change);
  for(const row of specFiles){const mode=(row.stat.mode&0o111)?'100755':'100644';records.push(`${row.logical}\x00${mode}\x00file\x00${sha(fs.readFileSync(row.full))}\x00`)}
  records.sort((a,b)=>Buffer.from(a).compare(Buffer.from(b)));
  return sha(Buffer.from(records.length?records.join(''):'<empty>\0'));
}
function verifyArchived(relative,change){
  const directory=lib.safeRepositoryEntry(root,relative,'archived campaign change','directory').path,name=path.basename(directory);
  const rootSnapshotEntry=lib.safeRepositoryEntry(root,'ai_snapshot.json','root workflow snapshot','file'),rootSnapshot=lib.parseJsonStrict(fs.readFileSync(rootSnapshotEntry.path,'utf8'),'root workflow snapshot');
  if(rootSnapshot.schema_version!==2||rootSnapshot.workflow!=='openspec'||
     !(rootSnapshot.active_change===null||changeId(rootSnapshot.active_change)))
    throw Error('root workflow snapshot identity mismatch');
  if(rootSnapshot.active_change===change||rootSnapshot.archive_failure?.change===change)throw Error('archived Campaign change is still active or under archive recovery: '+change);
  const receiptEntry=lib.safeRepositoryEntry(root,relative+'/harness/archive-receipt.json','archived Campaign receipt','file'),receipt=lib.parseJsonStrict(fs.readFileSync(receiptEntry.path,'utf8'),'archived Campaign receipt');
  closed(receipt,receiptKeys,'archived Campaign receipt');
  if(receipt.schema_version!==1||receipt.change_name!==change||receipt.archived_as!==name||
      receipt.authority!=='retained-receipt-not-active-state'||!Number.isFinite(Date.parse(receipt.archived_at))||
      receipt.archived_at.slice(0,10)!==name.slice(0,10)||
      receiptKeys.slice(4,10).some(key=>!digestPattern.test(receipt[key]))||
      receipt.artifact_fingerprint!==archivedArtifactFingerprint(relative,change))throw Error('archived Campaign receipt or retained artifact identity mismatch: '+change);
  const marker=fs.readFileSync(lib.safeRepositoryEntry(root,relative+'/.openspec.yaml','archived Campaign marker','file').path,'utf8');
  if(!/(?:^|\n)\s*schema:\s*spec-driven\s*(?:\n|$)/.test(marker))throw Error('archived Campaign schema marker mismatch: '+change);
  return {state:'archived',path:relative,archive_receipt_sha256:sha(fs.readFileSync(receiptEntry.path))};
}
function resolve(change){
  const activeRelative=`openspec/changes/${change}`,active=path.join(root,...activeRelative.split('/'));let hasActive=false;try{fs.lstatSync(active);hasActive=true}catch(error){if(error.code!=='ENOENT')throw error}
  const archiveEntry=lib.safeRepositoryEntry(root,'openspec/changes/archive','campaign archive','directory'),escaped=change.replace(/[.*+?^${}()|[\]\\]/g,'\\$&'),matches=fs.readdirSync(archiveEntry.path).filter(x=>new RegExp('^\\d{4}-\\d{2}-\\d{2}-'+escaped+'$').test(x));
  if((hasActive?1:0)+matches.length!==1)throw Error('campaign change is missing or has ambiguous active/archive identity: '+change);
  if(matches.length===1)return verifyArchived('openspec/changes/archive/'+matches[0],change);
  lib.safeRepositoryEntry(root,activeRelative,'active campaign change','directory');lib.safeRepositoryEntry(root,activeRelative+'/.openspec.yaml','active campaign marker','file');const validation=JSON.parse(cp.execFileSync(path.join(root,'scripts','openspec_cli.sh'),['validate',change,'--type','change','--strict','--json','--no-interactive'],{cwd:root,encoding:'utf8'})),items=validation?.items,failed=validation?.summary?.totals?.failed,bad=value=>{if(!value||typeof value!=='object')return false;if(value.valid===false||Array.isArray(value.issues)&&value.issues.some(issue=>String(issue?.level||issue?.severity||'').toUpperCase()==='ERROR'))return true;return Object.values(value).some(bad)};if(!Array.isArray(items)||items.length!==1||items[0]?.id!==change||items[0]?.valid!==true||!Number.isInteger(failed)||failed!==0||bad(validation))throw Error('active campaign change is not strict-valid: '+change);return {state:'active',path:activeRelative};
}
try{
  const campaignEntry=lib.safeRepositoryEntry(root,`.ai-harness/campaigns/${id}.json`,'campaign','file');
  const d=JSON.parse(fs.readFileSync(campaignEntry.path,'utf8'));closed(d,['schema_version','campaign_id','nodes','dependencies'],'campaign');if(d.schema_version!==1||d.campaign_id!==id||!Array.isArray(d.nodes)||!Array.isArray(d.dependencies))throw Error('campaign identity mismatch');
  const nodes=new Map;for(const [i,node]of d.nodes.entries()){closed(node,['change_id','weight'],`node[${i}]`);if(!changeId(node.change_id)||nodes.has(node.change_id)||!Number.isInteger(node.weight)||node.weight<1||node.weight>100000)throw Error('invalid campaign node');nodes.set(node.change_id,{...node,...resolve(node.change_id)})}
  const deps=new Map([...nodes.keys()].map(x=>[x,[]])),seenDependencyRows=new Set;for(const [i,row]of d.dependencies.entries()){closed(row,['change_id','depends_on'],`dependency[${i}]`);if(!nodes.has(row.change_id)||seenDependencyRows.has(row.change_id)||!Array.isArray(row.depends_on)||new Set(row.depends_on).size!==row.depends_on.length)throw Error('invalid campaign dependency row');seenDependencyRows.add(row.change_id);for(const dep of row.depends_on){if(!nodes.has(dep)||dep===row.change_id)throw Error('unknown/self campaign dependency')}deps.set(row.change_id,[...row.depends_on])}
  if(d.dependencies.length!==nodes.size)throw Error('every campaign node needs exactly one dependency row');
  const color=new Map,order=[];function visit(node){if(color.get(node)===1)throw Error('campaign dependency cycle');if(color.get(node)===2)return;color.set(node,1);for(const dep of deps.get(node))visit(dep);color.set(node,2);order.push(node)}for(const node of nodes.keys())visit(node);
  for(const [node,dependencies]of deps)if(nodes.get(node).state==='archived'&&dependencies.some(dependency=>nodes.get(dependency).state!=='archived'))throw Error('archived campaign node has an unfinished dependency: '+node);
  const dependents=new Map([...nodes.keys()].map(x=>[x,[]]));for(const [node,dependencies]of deps)for(const dependency of dependencies)dependents.get(dependency).push(node);
  const distance=new Map;for(const node of [...order].reverse())distance.set(node,nodes.get(node).weight+Math.max(0,...dependents.get(node).map(x=>distance.get(x))));
  const rows=[...nodes].map(([change,node])=>({change_id:change,state:node.state,path:node.path,archive_receipt_sha256:node.archive_receipt_sha256||null,weight:node.weight,depends_on:deps.get(change),ready:node.state==='active'&&deps.get(change).every(x=>nodes.get(x).state==='archived'),critical_path_weight:distance.get(change)})).sort((a,b)=>b.critical_path_weight-a.critical_path_weight||Buffer.from(a.change_id).compare(Buffer.from(b.change_id)));
  const ready=rows.filter(x=>x.ready),result={schema_version:1,campaign_id:id,config_sha256:sha(fs.readFileSync(campaignEntry.path)),status:rows.every(x=>x.state==='archived')?'complete':ready.length?'ready':'blocked',nodes:rows,next_ready:ready[0]?.change_id||null,side_effects:[]};
  if(action==='--next-ready'&&!result.next_ready)throw Error('campaign has no ready change');
  if(json)console.log(JSON.stringify(action==='--next-ready'?{schema_version:1,campaign_id:id,next_ready:result.next_ready,side_effects:[]}:result,null,2));else if(action==='--next-ready')console.log(result.next_ready);else console.log(`[OK] campaign ${id}: ${result.status}; next=${result.next_ready||'<none>'}`);
}catch(error){console.error('[ERR] '+error.message);process.exit(6)}

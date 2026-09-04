#!/usr/bin/env node
'use strict';
const fs=require('fs'),path=require('path'),crypto=require('crypto'),cp=require('child_process');
const profileLib=require('./project_profile_lib.js');
const root=process.cwd(),json=process.argv.includes('--json'),own=(o,k)=>Object.prototype.hasOwnProperty.call(o,k);
try{
  const file=path.join(root,'.ai-harness','workflow-contract.json'),st=fs.lstatSync(file);
  if(!st.isFile()||st.isSymbolicLink())throw Error('workflow contract must be a non-symlink regular file');
  const d=profileLib.parseJsonStrict(fs.readFileSync(file,'utf8'),'workflow contract');
  const closed=(o,keys,label)=>{if(!o||typeof o!=='object'||Array.isArray(o)||Object.keys(o).some(k=>!keys.includes(k))||keys.some(k=>!own(o,k)))throw Error(label+' schema mismatch')};
  const uniqueStrings=(value,label)=>{if(!Array.isArray(value)||value.some(x=>typeof x!=='string'||!x)||value.length!==new Set(value).size)throw Error(label+' must be a unique string array');return value};
  const sameSet=(left,right,label)=>{left=[...left].sort();right=[...right].sort();if(left.length!==right.length||left.some((x,i)=>x!==right[i]))throw Error(label+' mismatch')};
  const regularExecutable=(relative,label)=>{const p=path.join(root,...relative.split('/')),s=fs.lstatSync(p);if(!s.isFile()||s.isSymbolicLink()||(s.mode&0o111)===0)throw Error(label+' is missing or not executable: '+relative);return p};
  const canonical=value=>profileLib.canonical(value);
  const optionTokens=text=>[...new Set((text.match(/(^|[^A-Za-z0-9_-])(--[a-z0-9][a-z0-9-]*|-h)(?![A-Za-z0-9_-])/g)||[]).map(x=>x.match(/--[a-z0-9][a-z0-9-]*|-h/)[0]))].sort();
  const safeRegular=filePath=>{const s=fs.lstatSync(filePath);if(!s.isFile()||s.isSymbolicLink())throw Error('expected non-symlink regular file: '+filePath);return filePath};
  closed(d,['schema_version','workflow','setup_cli','state_ownership','roles','gates','artifact_ownership','schema_compatibility','schema_upgrades','managed_scripts','public_commands','prompt_references','documentation_references'],'workflow contract');
  closed(d.state_ownership,['planning','active_selector','project_capabilities','execution_evidence','final_verdict'],'state ownership');
  if(d.schema_version!==2||d.workflow!=='openspec'||d.state_ownership.planning!=='openspec'||d.state_ownership.active_selector!=='ai_snapshot.json'||d.state_ownership.final_verdict!=='evaluator')throw Error('workflow ownership identity mismatch');

  closed(d.setup_cli,['executable','default_mode','modes','options','constraints','test_discovery'],'setup CLI');
  if(d.setup_cli.executable!=='setup_ai_harness.sh'||d.setup_cli.default_mode!=='openspec')throw Error('setup CLI identity mismatch');
  const optionNames=new Set,canonicalOptions=new Set,optionGroups=[];
  if(!Array.isArray(d.setup_cli.options)||!d.setup_cli.options.length)throw Error('setup CLI options are missing');
  for(const option of d.setup_cli.options){
    closed(option,['canonical','aliases','arity','terminal'],'setup CLI option');
    if(!/^--[a-z0-9][a-z0-9-]*$/.test(option.canonical)||canonicalOptions.has(option.canonical)||![0,1].includes(option.arity)||typeof option.terminal!=='boolean')throw Error('invalid setup CLI option');
    canonicalOptions.add(option.canonical);uniqueStrings(option.aliases,'setup CLI aliases');
    const group=[option.canonical,...option.aliases];
    for(const name of group){if(!/^(?:--[a-z0-9][a-z0-9-]*|-h)$/.test(name)||optionNames.has(name))throw Error('duplicate/invalid setup CLI option name: '+name);optionNames.add(name)}
    optionGroups.push(group.sort().join('|'));
  }
  const modes=new Set,selectors=[];
  if(!Array.isArray(d.setup_cli.modes)||!d.setup_cli.modes.length)throw Error('setup CLI modes are missing');
  for(const mode of d.setup_cli.modes){
    closed(mode,['id','selector','read_only','compatible_options'],'setup CLI mode');
    if(!['openspec','detect','migration'].includes(mode.id)||modes.has(mode.id)||typeof mode.read_only!=='boolean'||(mode.selector!==null&&!canonicalOptions.has(mode.selector)))throw Error('invalid setup CLI mode');
    modes.add(mode.id);if(mode.selector!==null)selectors.push(mode.selector);
    for(const option of uniqueStrings(mode.compatible_options,'mode compatible options'))if(!canonicalOptions.has(option))throw Error('mode references unknown option: '+option);
  }
  sameSet(modes,['openspec','detect','migration'],'setup CLI modes');
  sameSet(selectors,['--detect-project','--migrate-openspec'],'setup CLI mode selectors');
  if(d.setup_cli.modes.find(x=>x.id==='detect')?.read_only!==true||d.setup_cli.modes.filter(x=>x.id!=='detect').some(x=>x.read_only))throw Error('setup CLI read-only mode contract mismatch');
  const expectedModes=[
    {id:'openspec',selector:null,read_only:false,compatible_options:['--force','--project-profile']},
    {id:'detect',selector:'--detect-project',read_only:true,compatible_options:['--json']},
    {id:'migration',selector:'--migrate-openspec',read_only:false,compatible_options:['--dry-run','--project-profile']}
  ].map(canonical);
  sameSet(d.setup_cli.modes.map(canonical),expectedModes,'setup CLI mode compatibility');
  const expectedConstraints=[
    ['json-requires-detect','requires',['--json','--detect-project']],
    ['dry-run-requires-migration','requires',['--dry-run','--migrate-openspec']],
    ['force-conflicts-migration','conflicts',['--force','--migrate-openspec']],
    ['detect-exclusive','exclusive-with-any',['--detect-project','--force','--migrate-openspec','--dry-run','--project-profile']]
  ].map(([id,kind,options])=>canonical({id,kind,options}));
  if(!Array.isArray(d.setup_cli.constraints))throw Error('setup CLI constraints are missing');
  const constraints=d.setup_cli.constraints.map(row=>{closed(row,['id','kind','options'],'setup CLI constraint');uniqueStrings(row.options,'setup CLI constraint options');for(const option of row.options)if(!canonicalOptions.has(option))throw Error('constraint references unknown option: '+option);return canonical(row)});
  sameSet(constraints,expectedConstraints,'setup CLI constraints');
  closed(d.setup_cli.test_discovery,['relative_directory','runner_function','negative_options'],'setup CLI test discovery');
  if(d.setup_cli.test_discovery.relative_directory!=='tests/openspec-integration/cases'||d.setup_cli.test_discovery.runner_function!=='run_setup')throw Error('setup CLI test discovery identity mismatch');
  uniqueStrings(d.setup_cli.test_discovery.negative_options,'negative setup CLI test options');

  if(!Array.isArray(d.roles)||d.roles.length!==3)throw Error('workflow roles drifted');const roleIds=[];
  for(const role of d.roles){closed(role,['id','inputs','outputs'],'role');if(!['planner','generator','evaluator'].includes(role.id)||roleIds.includes(role.id))throw Error('workflow role identity mismatch');roleIds.push(role.id);uniqueStrings(role.inputs,'role inputs');uniqueStrings(role.outputs,'role outputs')}sameSet(roleIds,['planner','generator','evaluator'],'workflow roles');
  const ids=new Set,scripts=new Set;
  for(const gate of d.gates){closed(gate,['id','phase','script','failure_mode'],'gate');if(ids.has(gate.id)||scripts.has(gate.script)||gate.failure_mode!=='fail-closed'||typeof gate.phase!=='string'||!gate.phase)throw Error('duplicate or invalid gate');ids.add(gate.id);scripts.add(gate.script)}
  if(!Array.isArray(d.artifact_ownership)||!d.artifact_ownership.length)throw Error('artifact ownership is missing');const ownedPaths=new Set;
  for(const row of d.artifact_ownership){closed(row,['path','owner','update_policy'],'artifact ownership');if(typeof row.path!=='string'||!row.path||ownedPaths.has(row.path)||typeof row.owner!=='string'||!row.owner||typeof row.update_policy!=='string'||!row.update_policy)throw Error('artifact ownership row is invalid');ownedPaths.add(row.path)}
  closed(d.schema_compatibility,['project_profile','root_snapshot','change_snapshot','verification','evaluation'],'schema compatibility');
  const expectedSchemas={project_profile:[1],root_snapshot:[2],change_snapshot:[2,3,4],verification:[1,2,3],evaluation:[1,2,3]};
  for(const [name,versions]of Object.entries(expectedSchemas))if(JSON.stringify(d.schema_compatibility[name])!==JSON.stringify(versions))throw Error('schema compatibility drift: '+name);
  const upgradeIds=new Set,upgradeOptions=new Set;
  if(!Array.isArray(d.schema_upgrades)||d.schema_upgrades.length!==2)throw Error('schema upgrades drifted');
  for(const upgrade of d.schema_upgrades){
    closed(upgrade,['id','script','option','from','to','eligibility'],'schema upgrade');
    closed(upgrade.from,['verification','change_snapshot'],'schema upgrade from');closed(upgrade.to,['verification','change_snapshot'],'schema upgrade to');
    if(upgradeIds.has(upgrade.id)||upgradeOptions.has(upgrade.option)||upgrade.script!=='scripts/task_verify.sh'||!['--upgrade-v2','--upgrade-v3'].includes(upgrade.option)||upgrade.eligibility!=='evidence-empty-only')throw Error('invalid schema upgrade');
    if(upgrade.to.verification!==upgrade.from.verification+1||upgrade.to.change_snapshot!==upgrade.from.change_snapshot+1||!expectedSchemas.verification.includes(upgrade.from.verification)||!expectedSchemas.verification.includes(upgrade.to.verification)||!expectedSchemas.change_snapshot.includes(upgrade.from.change_snapshot)||!expectedSchemas.change_snapshot.includes(upgrade.to.change_snapshot))throw Error('schema upgrade compatibility mismatch');
    upgradeIds.add(upgrade.id);upgradeOptions.add(upgrade.option);
  }
  sameSet(upgradeOptions,['--upgrade-v2','--upgrade-v3'],'schema upgrade options');
  uniqueStrings(d.managed_scripts,'managed scripts');for(const relative of d.managed_scripts)regularExecutable(relative,'managed script');
  const manifestFile=path.join(root,'.ai-harness','manifest.json'),manifestStat=fs.lstatSync(manifestFile),manifest=profileLib.parseJsonStrict(fs.readFileSync(manifestFile,'utf8'),'managed manifest');if(!manifestStat.isFile()||manifestStat.isSymbolicLink()||manifest.schema_version!==2||!Array.isArray(manifest.managed_paths))throw Error('managed manifest is unavailable for reverse parity');
  const manifestScripts=manifest.managed_paths.filter(x=>x?.ownership==='template'&&typeof x.path==='string'&&/^scripts\/[^/]+\.sh$/.test(x.path)).map(x=>x.path);
  sameSet(d.managed_scripts,manifestScripts,'contract/manifest managed script reverse parity');
  const publicSet=new Set;
  for(const relative of uniqueStrings(d.public_commands,'public commands')){if(!d.managed_scripts.includes(relative))throw Error('public command is not a managed implementation: '+relative);publicSet.add(relative);regularExecutable(relative,'public command')}
  for(const gate of d.gates){if(!d.managed_scripts.includes(gate.script))throw Error('gate implementation is not contract-managed: '+gate.id)}

  const factPatterns={
    'openspec-only-planning':/OpenSpec is the only source of truth/,
    'single-active-selector':/ai_snapshot\.json` is the only active-change selector/,
    'single-evaluator-verdict':/(?:one change-local Evaluation verdict|never creates a second verdict|only verdict|only final result)/,
    'project-profile-command-ids':/--project-command(?: <command-id>)?/,
    'generator-direct-evidence':/task_verify\.sh/,
    'evaluator-independent-evidence':/evaluator_check\.sh --run/,
    'planner-no-product-code':/Do not edit product code/,
    'strict-plan-check':/--plan-check/,
    'unplanned-surface-returns-planner':/unplanned surface[\s\S]{0,240}return to Planner/,
    'archive-wrapper-only':/scripts\/change_archive\.sh/,
    'archive-fail-closed':/partial archive failure/,
    'schema-upgrade-v2-explicit':/--upgrade-v2/,
    'schema-upgrade-v3-explicit':/--upgrade-v3/
  };
  const references=[...d.prompt_references.map(x=>({...x,kind:'prompt'})),...d.documentation_references.map(x=>({...x,kind:'documentation'}))];
  for(const ref of references){
    closed(ref,['path','role','facts','kind'],ref.kind+' reference');uniqueStrings(ref.facts,ref.kind+' facts');
    const p=path.join(root,...ref.path.split('/')),s=fs.lstatSync(p);if(!s.isFile()||s.isSymbolicLink())throw Error(ref.kind+' binding file is unavailable: '+ref.path);
    const text=fs.readFileSync(p,'utf8'),matches=[...text.matchAll(/<!-- autoai:workflow-binding:v1\s*\n([^\n]+)\n-->/g)];
    if(matches.length!==1)throw Error(ref.kind+' must contain exactly one workflow binding: '+ref.path);
    const binding=profileLib.parseJsonStrict(matches[0][1],ref.path+' workflow binding');closed(binding,['role','facts'],ref.path+' workflow binding');uniqueStrings(binding.facts,ref.path+' workflow facts');
    if(binding.role!==ref.role||canonical(binding.facts)!==canonical(ref.facts))throw Error(ref.kind+' structured binding drift: '+ref.path);
    for(const fact of ref.facts){if(!own(factPatterns,fact))throw Error('unknown workflow fact: '+fact);if(!factPatterns[fact].test(text))throw Error(ref.kind+' fact has no matching exported surface: '+ref.path+' -> '+fact)}
  }
  const scanFiles=[...new Set([...d.prompt_references,...d.documentation_references].map(x=>x.path))];
  for(const relative of scanFiles){const text=fs.readFileSync(path.join(root,...relative.split('/')),'utf8');for(const match of text.matchAll(/scripts\/[A-Za-z0-9_.-]+\.sh/g))if(!d.managed_scripts.includes(match[0]))throw Error('document exports an unmanaged script: '+relative+' -> '+match[0])}
  const taskVerify=fs.readFileSync(path.join(root,'scripts','task_verify.sh'),'utf8');
  const implementedUpgrades=[...taskVerify.matchAll(/^if \[\[ "\$\{1:-\}" == (--upgrade-v[0-9]+) \]\]; then$/gm)].map(x=>x[1]);
  sameSet(implementedUpgrades,[...upgradeOptions],'declared/implemented schema upgrade dispatch');

  let setupSource=process.env.AUTOAI_SETUP_SOURCE||null,setupParity={status:'not-applicable',reason:'setup source is not installed in this target checkout'},cliTests={status:'not-applicable',reason:'setup CLI tests are not installed in this target checkout'};
  if(setupSource){setupSource=path.resolve(setupSource);safeRegular(setupSource)}
  else{const local=path.join(root,d.setup_cli.executable);try{setupSource=safeRegular(local)}catch(error){if(error.code!=='ENOENT')throw error}}
  if(setupSource){
    const source=fs.readFileSync(setupSource,'utf8'),start=source.indexOf('while [ "$#" -gt 0 ]; do'),end=source.indexOf('# --- 颜色 ---');
    if(start<0||end<=start)throw Error('cannot structurally locate setup CLI parser');
    const parser=source.slice(start,end),armMatches=[...parser.matchAll(/^\s{8}((?:-{1,2}[a-z0-9][a-z0-9-]*)(?:\|(?:-{1,2}[a-z0-9][a-z0-9-]*))*)\)\n/gm)],groups=[],armBodies=new Map;
    const wildcard=parser.search(/^\s{8}\*\)\n/m);
    for(let i=0;i<armMatches.length;i++){
      const match=armMatches[i],key=match[1].split('|').sort().join('|'),bodyEnd=i+1<armMatches.length?armMatches[i+1].index:wildcard;
      if(bodyEnd<match.index)throw Error('cannot structurally delimit setup parser arm');
      groups.push(key);armBodies.set(key,parser.slice(match.index+match[0].length,bodyEnd));
    }
    sameSet(groups,optionGroups,'setup parser/contract option groups');
    if(!/^WORKFLOW_MODE="openspec"$/m.test(source.slice(0,start)))throw Error('setup default mode parser drift');
    const expectedModeAssignments={detect:'WORKFLOW_MODE="detect"',migration:'WORKFLOW_MODE="migration"'};
    for(const mode of d.setup_cli.modes)if(mode.selector!==null&&(!parser.includes(mode.selector)||!source.slice(0,end).includes(expectedModeAssignments[mode.id])))throw Error('setup mode parser drift: '+mode.id);
    const staticRules=[
      /if \[ "\$DRY_RUN" -eq 1 \] && \[ "\$MIGRATE_OPEN_SPEC" -ne 1 \]; then/,
      /if \[ "\$FORCE" -eq 1 \] && \[ "\$MIGRATE_OPEN_SPEC" -eq 1 \]; then/,
      /if \[ "\$OUTPUT_JSON" -eq 1 \] && \[ "\$DETECT_PROJECT" -ne 1 \]; then/,
      /if \[ "\$DETECT_PROJECT" -eq 1 \]; then[\s\S]*?"\$FORCE" -eq 1[\s\S]*?"\$MIGRATE_OPEN_SPEC" -eq 1[\s\S]*?"\$DRY_RUN" -eq 1[\s\S]*?-n "\$PROJECT_PROFILE_PATH"/
    ];
    if(staticRules.some(rule=>!rule.test(source.slice(0,end))))throw Error('setup mode constraint parser drift');
    const armFor=option=>armBodies.get([option.canonical,...option.aliases].sort().join('|'))||'';
    for(const option of d.setup_cli.options.filter(x=>x.arity===1)){
      const arm=armFor(option);
      if(!arm||!/"\$#" -ge 2/.test(arm)||!/\n\s*shift\s*\n/.test(arm))throw Error('setup parser arity drift: '+option.canonical);
    }
    for(const option of d.setup_cli.options.filter(x=>x.arity===0))
      if(/^\s*shift\s*$/m.test(armFor(option)))throw Error('zero-arity setup option consumes an argument: '+option.canonical);
    const usage=source.match(/usage\(\) \{\n\s*cat <<'EOF'\n([\s\S]*?)\nEOF\n\}/);
    if(!usage)throw Error('cannot structurally locate setup help source');
    sameSet(optionTokens(usage[1]),[...optionNames],'setup help/contract option surface');
    if(!/^AUTOAI_HARNESS_VERSION="[0-9]+\.[0-9]+\.[0-9]+"$/m.test(source.slice(0,start))||!parser.includes("printf 'AutoAI Harness %s\\n' \"$AUTOAI_HARNESS_VERSION\""))throw Error('setup version terminal output drift');
    for(const option of d.setup_cli.options.filter(x=>x.terminal)){
      const arm=armFor(option);
      if(!arm||!/\n\s*exit 0\s*\n/.test(arm))throw Error('setup terminal option drift: '+option.canonical);
    }
    if(!/^\s*usage\s*$/m.test(armFor(d.setup_cli.options.find(x=>x.canonical==='--help'))))
      throw Error('setup help terminal no longer calls usage');
    setupParity={status:'pass',source:setupSource,parser_options:[...optionNames].sort(),modes:[...modes].sort(),constraints:d.setup_cli.constraints.map(x=>x.id).sort()};
    const testDir=process.env.AUTOAI_SETUP_TEST_ROOT||path.join(path.dirname(setupSource),...d.setup_cli.test_discovery.relative_directory.split('/'));
    try{
      const testStat=fs.lstatSync(testDir);
      if(!testStat.isDirectory()||testStat.isSymbolicLink())throw Error('setup CLI test directory must be a non-symlink directory');
      const files=fs.readdirSync(testDir).filter(x=>/^test_.*\.sh$/.test(x)).sort(),observed=new Set;
      for(const name of files){
        const text=fs.readFileSync(safeRegular(path.join(testDir,name)),'utf8').replace(/\\\n/g,' ');
        for(const line of text.split('\n'))if(new RegExp('\\b'+d.setup_cli.test_discovery.runner_function+'\\b').test(line))for(const token of optionTokens(line))observed.add(token);
      }
      const allowed=new Set([...optionNames,...d.setup_cli.test_discovery.negative_options]),undeclared=[...observed].filter(x=>!allowed.has(x));
      if(undeclared.length)throw Error('setup CLI tests enumerate undeclared options: '+undeclared.join(', '));
      const covered=[...canonicalOptions].filter(x=>observed.has(x)),missing=[...canonicalOptions].filter(x=>!observed.has(x));
      if(missing.length)throw Error('setup CLI tests do not enumerate canonical options: '+missing.sort().join(', '));
      cliTests={status:'pass',directory:testDir,covered_options:covered.sort(),not_enumerated:[],negative_options:[...observed].filter(x=>d.setup_cli.test_discovery.negative_options.includes(x)).sort()};
    }catch(error){if(error.code!=='ENOENT')throw error}
  }
  const contractSha='sha256:'+crypto.createHash('sha256').update(Buffer.from(JSON.stringify(d))).digest('hex');
  if(json)console.log(JSON.stringify({schema_version:2,status:'pass',contract_sha256:contractSha,gates:[...ids].sort(),managed_scripts:[...d.managed_scripts].sort(),public_commands:[...publicSet].sort(),schema_compatibility:d.schema_compatibility,schema_upgrades:[...upgradeIds].sort(),setup_cli:setupParity,setup_cli_tests:cliTests},null,2));
  else console.log('[OK] workflow contract '+contractSha);
}catch(error){console.error('[ERR] '+error.message);process.exit(6)}

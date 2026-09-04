#!/usr/bin/env node
'use strict';

const fs = require('fs');
const path = require('path');
const crypto = require('crypto');
const cp = require('child_process');
const scopeLib = require('./change_scope.js');

const DIGEST = /^sha256:[0-9a-f]{64}$/;
const CHANGE_ID = /^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$/;
const SURFACE_ID = /^surface-[a-z0-9]+(?:-[a-z0-9]+)*$/;
const PROBE_ID = /^probe-[a-z0-9]+(?:-[a-z0-9]+)*$/;
const TASK_ID = /^\d+(?:\.\d+)*$/;
const KINDS = ['internal_api', 'external_api', 'callback_or_plugin', 'cli', 'configuration', 'protocol_or_persistence', 'build_or_install'];
const VERIFY_KINDS = ['build', 'test', 'behavior', 'static'];
const ROLE_ORDER = ['current', 'old_consumer', 'replacement_consumer', 'absence_probe'];
const CONSUMER_BY_KIND = {
    internal_api: 'production_caller', external_api: 'representative_external',
    callback_or_plugin: 'registration_dispatch', cli: 'real_entrypoint',
    configuration: 'real_entrypoint', protocol_or_persistence: 'producer_consumer_pair',
    build_or_install: 'downstream_build'
};
const SYMBOL_FIELDS = ['declaration_kind', 'qualified_name', 'canonical_parameter_types', 'canonical_return_type', 'template_parameter_kinds', 'cv_qualifiers', 'ref_qualifier', 'declaration_path'];
const SURFACE_FIELDS = ['change_kind', 'compatibility', 'consumer_kind', 'consumer_paths', 'contract_impact', 'entrypoint', 'evidence_contracts', 'expected_observation', 'id', 'kind', 'name', 'producer_paths', 'requirement_refs', 'symbol_identities', 'task_ids', 'task_obligations', 'verify_kinds'];
const BUILD_SURFACE_FIELDS = [...SURFACE_FIELDS, 'runnable_artifact'];
const PROBE_FIELDS = ['argv', 'expected_exit_codes', 'kind', 'output_contains', 'probe_id', 'role'];
const CREDENTIAL_OPTIONS = new Set(['--token', '--password', '--secret', '--api-key', '--apikey', '-H', '--header', '--cookie']);
const VERIFICATION_WORKSPACE_PREFIX = '.ai-harness/logs/verification-workspaces';
const referencesVerificationWorkspace = value => typeof value === 'string' &&
    (value.includes('AUTOAI_VERIFY_TMPDIR') || /(?:^|[/\\])\.ai-harness[/\\]logs[/\\]verification-workspaces(?:[/\\]|$)/.test(value));
const sha = value => 'sha256:' + crypto.createHash('sha256').update(value).digest('hex');
const cmp = (a, b) => Buffer.from(a).compare(Buffer.from(b));
const canonical = value => Array.isArray(value) ? '[' + value.map(canonical).join(',') + ']'
    : value && typeof value === 'object' ? '{' + Object.keys(value).sort(cmp).map(k => JSON.stringify(k) + ':' + canonical(value[k])).join(',') + '}'
    : JSON.stringify(value);
const own = (o, k) => Object.prototype.hasOwnProperty.call(o, k);
const closed = (o, fields, label) => {
    if (!o || typeof o !== 'object' || Array.isArray(o) || Object.keys(o).some(k => !fields.includes(k)) || fields.some(k => !own(o, k))) {
        throw Error(label + ' schema mismatch');
    }
};
const nonempty = (v, label) => {
    if (typeof v !== 'string' || !v.trim() || /[\0\r\n]/.test(v)) throw Error(label + ' must be non-empty and single-line');
    return v;
};
const SECRET_LIKE = /(?:authorization|proxy-authorization|bearer|x-?api-?key|api[_-]?key|token|password|secret|cookie|client[_-]?secret|private[_-]?key|access[_-]?key)[\s:=]+\S+|:\/\/[^/\s:]+:[^/@\s]+@|-----BEGIN [A-Z ]*PRIVATE KEY-----/i;
const safeText = (v, label) => {
    nonempty(v, label);
    if (v.length > 500 || SECRET_LIKE.test(v)) throw Error(label + ' is too long or contains secret-like material');
    return v;
};
const unique = (values, label) => {
    if (!Array.isArray(values) || values.length !== new Set(values.map(canonical)).size) throw Error(label + ' must be a unique array');
    return values;
};
const exactSet = (actual, expected, label) => {
    unique(actual, label); unique(expected, label + ' expected');
    if (actual.length !== expected.length || actual.some(x => !expected.includes(x))) throw Error(label + ' coverage mismatch');
};
const safePath = (p, label = 'path') => {
    scopeLib.safePath(p, label);
    if (p.includes('*') || p.includes('?')) throw Error(label + ' cannot be a glob');
    if (p === VERIFICATION_WORKSPACE_PREFIX || p.startsWith(VERIFICATION_WORKSPACE_PREFIX + '/')) throw Error(label + ' cannot use the temporary verification workspace');
    return p;
};
const taskCmp = (a, b) => {
    const aa = a.split('.').map(Number), bb = b.split('.').map(Number);
    for (let i = 0; i < Math.max(aa.length, bb.length); i++) { const d = (aa[i] || 0) - (bb[i] || 0); if (d) return d; }
    return 0;
};

function parseJsonNoDuplicates(source) {
    let index = 0;
    const ws = () => { while (/\s/.test(source[index] || '')) index++; };
    const string = () => { const start = index++; for (; index < source.length; index++) { if (source[index] === '\\') { index++; continue; } if (source[index] === '"') { index++; return JSON.parse(source.slice(start, index)); } } throw Error('unterminated JSON string'); };
    const value = () => {
        ws(); const c = source[index];
        if (c === '"') return string();
        if (c === '{') { index++; const out = {}, seen = new Set; ws(); if (source[index] === '}') { index++; return out; } for (;;) { ws(); if (source[index] !== '"') throw Error('JSON key expected'); const key = string(); if (seen.has(key)) throw Error('duplicate JSON key: ' + key); seen.add(key); ws(); if (source[index++] !== ':') throw Error('JSON colon expected'); out[key] = value(); ws(); const d = source[index++]; if (d === '}') return out; if (d !== ',') throw Error('JSON comma expected'); } }
        if (c === '[') { index++; const out = []; ws(); if (source[index] === ']') { index++; return out; } for (;;) { out.push(value()); ws(); const d = source[index++]; if (d === ']') return out; if (d !== ',') throw Error('JSON comma expected'); } }
        const match = source.slice(index).match(/^(?:true|false|null|-?(?:0|[1-9]\d*)(?:\.\d+)?(?:[eE][+-]?\d+)?)/);
        if (!match) throw Error('invalid JSON'); index += match[0].length; return JSON.parse(match[0]);
    };
    const result = value(); ws(); if (index !== source.length) throw Error('trailing JSON'); return result;
}

function extractBlock(designText, start, end, label) {
    if (designText.split(start).length !== 2 || designText.split(end).length !== 2) throw Error('exactly one ' + label + ' block required');
    const begin = designText.indexOf(start) + start.length, finish = designText.indexOf(end);
    if (finish < begin) throw Error(label + ' markers are out of order');
    const body = designText.slice(begin, finish), match = body.match(/^\s*```json\s*\n([\s\S]*?)\n```\s*$/);
    if (!match) throw Error(label + ' must contain one JSON fence');
    return parseJsonNoDuplicates(match[1]);
}

function normalizeRef(ref) {
    const required = ['spec_path', 'operation', 'requirement', 'scenarios'];
    const fields = ref?.operation === 'RENAMED' ? [...required, 'renamed_to'] : required;
    closed(ref, fields, 'requirement reference'); safePath(ref.spec_path, 'spec_path');
    if (!ref.spec_path.startsWith('specs/') || !ref.spec_path.endsWith('/spec.md') || !['ADDED', 'MODIFIED', 'REMOVED', 'RENAMED'].includes(ref.operation)) throw Error('invalid requirement reference');
    nonempty(ref.requirement, 'requirement'); unique(ref.scenarios, 'requirement scenarios');
    if (ref.scenarios.some(x => typeof x !== 'string' || !x || /[\r\n]/.test(x))) throw Error('invalid scenario');
    if (['ADDED', 'MODIFIED'].includes(ref.operation) && ref.scenarios.length !== 1) throw Error('behavior requirement reference needs exactly one scenario');
    if (['REMOVED', 'RENAMED'].includes(ref.operation) && ref.scenarios.length !== 0) throw Error('operation cannot carry scenarios');
    if (ref.operation === 'RENAMED') nonempty(ref.renamed_to, 'renamed_to');
    return { spec_path: ref.spec_path, operation: ref.operation, requirement: ref.requirement, scenarios: [...ref.scenarios], ...(ref.operation === 'RENAMED' ? { renamed_to: ref.renamed_to } : {}) };
}
const refKey = ref => canonical(normalizeRef(ref));

function parseTasks(tasksText) {
    const lines = tasksText.split(/\r?\n/), tasks = new Map;
    for (let i = 0; i < lines.length; i++) {
        const match = lines[i].match(/^- \[([ xX])\] (\d+(?:\.\d+)*)\s+(.+)$/); if (!match) continue;
        if (tasks.has(match[2])) throw Error('duplicate task ID: ' + match[2]);
        const task = { id: match[2], done: match[1] !== ' ', refs: [], verify: [] };
        for (let j = i + 1; j < lines.length && !/^- \[[ xX]\] \d/.test(lines[j]); j++) {
            const cover = lines[j].match(/^\s+- Covers: `([^`]+)` \| `(ADDED|MODIFIED|REMOVED|RENAMED)` \| `([^`]+)` \| `([^`]+)`\s*$/);
            if (cover) {
                let requirement = cover[3], renamedTo;
                if (cover[2] === 'RENAMED') { const pair = requirement.split(' -> '); if (pair.length !== 2) throw Error('invalid RENAMED Covers'); [requirement, renamedTo] = pair; }
                task.refs.push(normalizeRef({ spec_path: cover[1], operation: cover[2], requirement, scenarios: cover[4] === '<none>' ? [] : [cover[4]], ...(renamedTo ? { renamed_to: renamedTo } : {}) }));
            }
            const verify = lines[j].match(/^\s+- Verify:\s*(.+)$/);
            if (verify) task.verify = [...verify[1].matchAll(/`(build|test|behavior|static)`/g)].map(x => x[1]);
        }
        if (!task.refs.length || !task.verify.length || task.verify.length !== new Set(task.verify).size) throw Error('task metadata incomplete: ' + task.id);
        task.refKeys = task.refs.map(refKey); if (task.refKeys.length !== new Set(task.refKeys).size) throw Error('duplicate task Covers: ' + task.id);
        tasks.set(task.id, task);
    }
    if (!tasks.size) throw Error('at least one leaf task required'); return tasks;
}

function visibleLines(file) {
    const result = []; let fence = null;
    for (const line of fs.readFileSync(file, 'utf8').split(/\r?\n/)) {
        const marker = line.match(/^\s*(`{3,}|~{3,})/);
        if (marker) { if (!fence) fence = { c: marker[1][0], n: marker[1].length }; else if (marker[1][0] === fence.c && marker[1].length >= fence.n) fence = null; continue; }
        if (!fence) result.push(line);
    }
    if (fence) throw Error('unclosed Markdown fence: ' + file); return result;
}

function parseDeltaUniverse(changeRoot) {
    const specsRoot = path.join(changeRoot, 'specs'), files = [];
    const walk = dir => { const st = fs.lstatSync(dir); if (!st.isDirectory() || st.isSymbolicLink()) throw Error('unsafe delta specs'); for (const name of fs.readdirSync(dir).sort(cmp)) { const p = path.join(dir, name), s = fs.lstatSync(p); if (s.isSymbolicLink()) throw Error('delta spec symlink'); if (s.isDirectory()) walk(p); else if (s.isFile() && name === 'spec.md') files.push(p); } };
    walk(specsRoot); if (!files.length) throw Error('no delta specs');
    const refs = [];
    for (const file of files) {
        const specPath = path.relative(changeRoot, file).split(path.sep).join('/'); let operation = null, current = null, renameFrom = null;
        const emit = () => { if (!current) return; if (!['ADDED', 'MODIFIED', 'REMOVED'].includes(operation)) throw Error('invalid requirement section'); if (['ADDED', 'MODIFIED'].includes(operation)) { if (!current.scenarios.length) throw Error('delta requirement needs scenario'); for (const scenario of current.scenarios) refs.push(normalizeRef({ spec_path: specPath, operation, requirement: current.name, scenarios: [scenario] })); } else { if (current.scenarios.length) throw Error('removed requirement cannot carry scenario'); refs.push(normalizeRef({ spec_path: specPath, operation, requirement: current.name, scenarios: [] })); } current = null; };
        for (const line of visibleLines(file)) {
            const section = line.match(/^##\s+(ADDED|MODIFIED|REMOVED|RENAMED)\s+Requirements\s*$/i);
            if (section) { emit(); if (renameFrom) throw Error('unpaired rename'); operation = section[1].toUpperCase(); continue; }
            if (!operation) continue;
            const from = line.match(/^\s*-\s+FROM:\s+`###\s+Requirement:\s*([^`]+?)`\s*$/i), to = line.match(/^\s*-\s+TO:\s+`###\s+Requirement:\s*([^`]+?)`\s*$/i);
            if (from || to) { emit(); if (operation !== 'RENAMED') throw Error('rename outside RENAMED section'); if (from) { if (renameFrom) throw Error('duplicate rename FROM'); renameFrom = from[1].trim(); } else { if (!renameFrom) throw Error('rename TO without FROM'); refs.push(normalizeRef({ spec_path: specPath, operation: 'RENAMED', requirement: renameFrom, scenarios: [], renamed_to: to[1].trim() })); renameFrom = null; } continue; }
            const bullet = line.match(/^\s*-\s+`###\s+Requirement:\s*([^`]+?)`\s*$/i), header = line.match(/^###\s+Requirement:\s*(.+?)\s*$/i);
            if (bullet || header) { emit(); current = { name: (bullet || header)[1].trim(), scenarios: [] }; continue; }
            const scenario = line.match(/^####\s+Scenario:\s*(.+?)\s*$/i); if (scenario) { if (!current) throw Error('orphan scenario'); current.scenarios.push(scenario[1].trim()); }
        }
        emit(); if (renameFrom) throw Error('unpaired rename');
    }
    const keys = refs.map(refKey); if (keys.length !== new Set(keys).size) throw Error('duplicate delta reference'); return new Set(keys);
}

function requiredRoles(surface) {
    if (surface.contract_impact === 'compatible') return ['current'];
    if (['breaking', 'deprecation'].includes(surface.contract_impact)) return ['old_consumer', 'replacement_consumer'];
    return ['old_consumer', ...(surface.compatibility.replacement_consumer_paths.length ? ['replacement_consumer'] : []), 'absence_probe'];
}

function normalizeSymbolIdentity(identity) {
    closed(identity, SYMBOL_FIELDS, 'symbol identity');
    if (!['function', 'method', 'constructor', 'destructor', 'conversion', 'operator', 'type', 'function_template', 'class_template'].includes(identity.declaration_kind)) throw Error('invalid declaration_kind');
    nonempty(identity.qualified_name, 'qualified_name'); safePath(identity.declaration_path, 'declaration_path');
    // Parameter/template lists are ordered sequences, not sets: f(int, int)
    // and template<class, class> are both valid identities.  Only the
    // qualifier list is a canonical set.
    if (!Array.isArray(identity.canonical_parameter_types) || !Array.isArray(identity.template_parameter_kinds)) throw Error('invalid identity array');
    unique(identity.cv_qualifiers, 'cv_qualifiers');
    if (identity.canonical_parameter_types.some(x => typeof x !== 'string' || !x) || identity.template_parameter_kinds.some(x => typeof x !== 'string' || !x)) throw Error('invalid identity array');
    if (identity.cv_qualifiers.some(x => !['const', 'volatile'].includes(x)) || identity.cv_qualifiers.join(',') !== identity.cv_qualifiers.slice().sort((a, b) => ['const', 'volatile'].indexOf(a) - ['const', 'volatile'].indexOf(b)).join(',')) throw Error('invalid cv qualifiers');
    if (!['none', 'lvalue', 'rvalue'].includes(identity.ref_qualifier)) throw Error('invalid ref qualifier');
    if (['constructor', 'destructor', 'type', 'class_template'].includes(identity.declaration_kind)) { if (identity.canonical_return_type !== null) throw Error('identity return type must be null'); } else if (typeof identity.canonical_return_type !== 'string') throw Error('canonical return type required');
    return { ...identity, canonical_parameter_types: [...identity.canonical_parameter_types], template_parameter_kinds: [...identity.template_parameter_kinds], cv_qualifiers: [...identity.cv_qualifiers] };
}

function normalizePlan(block, tasks, deltaUniverse) {
    closed(block, ['discovery', 'schema_version', 'surfaces'], 'Integration Completeness v1');
    closed(block.discovery, ['compile_commands_path', 'mode'], 'Integration discovery');
    if (block.schema_version !== 1 || !['reviewed_inventory', 'clang_ast'].includes(block.discovery.mode) || !Array.isArray(block.surfaces)) throw Error('invalid Integration Completeness header');
    if (block.discovery.mode === 'reviewed_inventory' && block.discovery.compile_commands_path !== null) throw Error('reviewed_inventory compile_commands_path must be null');
    if (block.discovery.mode === 'clang_ast') {
        safePath(block.discovery.compile_commands_path, 'compile_commands_path');
        if (!block.discovery.compile_commands_path.endsWith('.json')) throw Error('clang_ast compile_commands_path must name a .json file');
    }
    const surfaceIds = new Set, probeIds = new Set;
    const identityOwners = { base: new Map, current: new Map };
    const normalized = [];
    for (const raw of block.surfaces) {
        closed(raw, raw.kind === 'build_or_install' ? BUILD_SURFACE_FIELDS : SURFACE_FIELDS, 'surface');
        if (!SURFACE_ID.test(raw.id) || surfaceIds.has(raw.id)) throw Error('invalid or duplicate surface ID'); surfaceIds.add(raw.id);
        if (!KINDS.includes(raw.kind) || raw.consumer_kind !== (raw.contract_impact === 'removal' ? 'compatibility_probe' : CONSUMER_BY_KIND[raw.kind])) throw Error('surface kind/consumer mismatch: ' + raw.id);
        if (!['added', 'modified', 'deprecated', 'removed'].includes(raw.change_kind) || !['compatible', 'breaking', 'deprecation', 'removal'].includes(raw.contract_impact)) throw Error('surface change/impact invalid');
        if (raw.change_kind === 'deprecated' ? raw.contract_impact !== 'deprecation' : raw.change_kind === 'removed' ? raw.contract_impact !== 'removal' : !['compatible', 'breaking'].includes(raw.contract_impact)) throw Error('surface change/impact pair invalid');
        safeText(raw.name, 'surface name'); safeText(raw.entrypoint, 'entrypoint'); safeText(raw.expected_observation, 'expected observation');
        for (const [label, values, allowEmpty] of [['producer_paths', raw.producer_paths, false], ['consumer_paths', raw.consumer_paths, false]]) { unique(values, label); if (!allowEmpty && !values.length) throw Error(label + ' cannot be empty'); values.forEach(p => safePath(p, label)); }
        let compatibility = null;
        if (raw.contract_impact === 'compatible') { if (raw.compatibility !== null) throw Error('compatible surface cannot carry compatibility'); }
        else {
            closed(raw.compatibility, ['old_consumer_paths', 'replacement_consumer_paths', 'replacement_policy', 'expected_old_result', 'migration_path', 'exit_condition'], 'compatibility');
            unique(raw.compatibility.old_consumer_paths, 'old consumer paths'); unique(raw.compatibility.replacement_consumer_paths, 'replacement consumer paths');
            if (!['required', 'requirement_approved_none'].includes(raw.compatibility.replacement_policy)) throw Error('compatibility replacement policy invalid');
            const hasReplacement = raw.compatibility.replacement_consumer_paths.length > 0;
            if (!raw.compatibility.old_consumer_paths.length ||
                (raw.compatibility.replacement_policy === 'required') !== hasReplacement ||
                (raw.compatibility.replacement_policy === 'requirement_approved_none' && raw.contract_impact !== 'removal')) {
                throw Error('compatibility replacement policy/path mismatch');
            }
            [...raw.compatibility.old_consumer_paths, ...raw.compatibility.replacement_consumer_paths].forEach(p => safePath(p, 'compatibility path'));
            safeText(raw.compatibility.expected_old_result, 'expected old result'); safeText(raw.compatibility.migration_path, 'migration path'); safeText(raw.compatibility.exit_condition, 'exit condition');
            compatibility = { ...raw.compatibility, old_consumer_paths: [...raw.compatibility.old_consumer_paths].sort(cmp), replacement_consumer_paths: [...raw.compatibility.replacement_consumer_paths].sort(cmp) };
        }
        unique(raw.requirement_refs, 'requirement refs'); if (!raw.requirement_refs.length) throw Error('surface requirement refs required'); const refs = raw.requirement_refs.map(normalizeRef); const refKeys = refs.map(refKey); if (refKeys.some(k => !deltaUniverse.has(k))) throw Error('surface requirement is not an exact delta reference');
        if (compatibility?.replacement_policy === 'requirement_approved_none' && refs.some(ref => ref.operation !== 'REMOVED')) throw Error('no-replacement retirement requires only REMOVED requirement references');
        unique(raw.task_ids, 'task IDs'); if (!raw.task_ids.length || raw.task_ids.some(id => !TASK_ID.test(id) || !tasks.has(id))) throw Error('surface task ID invalid');
        unique(raw.verify_kinds, 'surface verify kinds'); if (!raw.verify_kinds.length || raw.verify_kinds.some(k => !VERIFY_KINDS.includes(k))) throw Error('invalid surface verify kinds');
        if (raw.kind !== 'build_or_install' && !raw.verify_kinds.some(k => ['test', 'behavior'].includes(k))) throw Error('surface needs test or behavior evidence');
        if (raw.kind === 'build_or_install') {
            if (typeof raw.runnable_artifact !== 'boolean') throw Error('build surface runnable_artifact must be boolean');
            if (!raw.verify_kinds.includes('build')) throw Error('build surface needs downstream build evidence');
            if (raw.runnable_artifact && !raw.verify_kinds.some(k => ['test', 'behavior'].includes(k))) throw Error('runnable build surface needs runtime evidence');
        }
        unique(raw.task_obligations, 'task obligations'); if (!raw.task_obligations.length) throw Error('task obligations required');
        const obligationByTask = new Map;
        for (const item of raw.task_obligations) {
            closed(item, ['evidence_roles', 'task_id', 'verify_kinds'], 'task obligation');
            if (!raw.task_ids.includes(item.task_id) || obligationByTask.has(item.task_id)) throw Error('task obligation identity mismatch');
            unique(item.verify_kinds, 'obligation verify kinds'); unique(item.evidence_roles, 'obligation roles');
            if (!!item.verify_kinds.length !== !!item.evidence_roles.length || item.verify_kinds.some(k => !raw.verify_kinds.includes(k) || !tasks.get(item.task_id).verify.includes(k)) || item.evidence_roles.some(r => !ROLE_ORDER.includes(r))) throw Error('task obligation invalid');
            obligationByTask.set(item.task_id, { task_id: item.task_id, verify_kinds: [...item.verify_kinds].sort((a, b) => VERIFY_KINDS.indexOf(a) - VERIFY_KINDS.indexOf(b)), evidence_roles: [...item.evidence_roles].sort((a, b) => ROLE_ORDER.indexOf(a) - ROLE_ORDER.indexOf(b)) });
        }
        exactSet([...obligationByTask.keys()], raw.task_ids, 'task obligations');
        exactSet([...new Set([...obligationByTask.values()].flatMap(x => x.verify_kinds))], raw.verify_kinds, 'obligation kinds'); const roles = requiredRoles(raw); exactSet([...new Set([...obligationByTask.values()].flatMap(x => x.evidence_roles))], roles, 'obligation roles');
        const obligationPairs = [...new Set([...obligationByTask.values()].flatMap(item => item.verify_kinds.flatMap(kind => item.evidence_roles.map(role => kind + '\0' + role))))];
        exactSet(obligationPairs, raw.verify_kinds.flatMap(kind => roles.map(role => kind + '\0' + role)), 'task obligation kind/role pairs');
        unique(raw.evidence_contracts, 'evidence contracts');
        const evidenceContracts = [], probePairs = new Set;
        for (const probe of raw.evidence_contracts) {
            closed(probe, PROBE_FIELDS, 'evidence contract');
            if (!PROBE_ID.test(probe.probe_id) || probeIds.has(probe.probe_id)) throw Error('invalid or duplicate probe ID');
            probeIds.add(probe.probe_id);
            if (!raw.verify_kinds.includes(probe.kind) || !roles.includes(probe.role)) throw Error('evidence contract kind/role is not assigned to the surface');
            const pair = probe.kind + '\0' + probe.role; if (probePairs.has(pair)) throw Error('duplicate evidence contract kind/role'); probePairs.add(pair);
            if (!Array.isArray(probe.argv) || !probe.argv.length || typeof probe.argv[0] !== 'string' || !probe.argv[0] || probe.argv.some(arg => typeof arg !== 'string' || /[\0\r\n]/.test(arg) || CREDENTIAL_OPTIONS.has(arg) || SECRET_LIKE.test(arg) || referencesVerificationWorkspace(arg))) throw Error('evidence contract argv is invalid, secret-like, or depends on retained temporary verification material');
            unique(probe.expected_exit_codes, 'evidence contract expected exits');
            if (!probe.expected_exit_codes.length || probe.expected_exit_codes.some(code => !Number.isInteger(code) || code < 0 || code > 255) || probe.expected_exit_codes.some((code, index) => index > 0 && probe.expected_exit_codes[index - 1] >= code)) throw Error('evidence contract expected exits must be canonical shell exit codes');
            safeText(probe.output_contains, 'evidence contract output_contains');
            evidenceContracts.push({ probe_id: probe.probe_id, kind: probe.kind, role: probe.role, argv: [...probe.argv], expected_exit_codes: [...probe.expected_exit_codes], output_contains: probe.output_contains });
        }
        const requiredProbePairs = raw.verify_kinds.flatMap(kind => roles.map(role => kind + '\0' + role));
        exactSet([...probePairs], requiredProbePairs, 'evidence contract kind/role');
        evidenceContracts.sort((a, b) => VERIFY_KINDS.indexOf(a.kind) - VERIFY_KINDS.indexOf(b.kind) || ROLE_ORDER.indexOf(a.role) - ROLE_ORDER.indexOf(b.role) || cmp(a.probe_id, b.probe_id));
        for (const taskId of raw.task_ids) {
            const taskRefs = tasks.get(taskId).refKeys, intersects = refKeys.some(k => taskRefs.includes(k)); if (!intersects) throw Error('surface task has no Covers edge: ' + taskId);
        }
        for (const key of refKeys) if (!raw.task_ids.some(id => tasks.get(id).refKeys.includes(key))) throw Error('surface requirement has no task Covers edge');
        let symbols = null;
        if (block.discovery.mode === 'reviewed_inventory') { if (raw.symbol_identities !== null) throw Error('reviewed surface cannot carry symbol identities'); }
        else if (raw.symbol_identities !== null) {
            closed(raw.symbol_identities, ['base', 'current'], 'symbol identities'); unique(raw.symbol_identities.base, 'base identities'); unique(raw.symbol_identities.current, 'current identities');
            const base = raw.symbol_identities.base.map(normalizeSymbolIdentity), current = raw.symbol_identities.current.map(normalizeSymbolIdentity);
            if (raw.change_kind === 'added' ? base.length || !current.length : raw.change_kind === 'removed' ? !base.length || current.length : !base.length || !current.length) throw Error('symbol identities do not match change kind');
            for (const [side, identities] of [['base', base], ['current', current]]) for (const identity of identities) {
                const key = canonical(identity), owners = identityOwners[side];
                if (owners.has(key) && owners.get(key) !== raw.id) throw Error('symbol identity belongs to multiple surfaces on the same tree side');
                owners.set(key, raw.id);
            }
            symbols = { base: base.sort((a, b) => cmp(canonical(a), canonical(b))), current: current.sort((a, b) => cmp(canonical(a), canonical(b))) };
        }
        if (block.discovery.mode === 'clang_ast' && ['internal_api', 'external_api'].includes(raw.kind) && symbols === null) throw Error('clang_ast C++ API surface requires symbol identities: ' + raw.id);
        normalized.push({ change_kind: raw.change_kind, compatibility, consumer_kind: raw.consumer_kind, consumer_paths: [...raw.consumer_paths].sort(cmp), contract_impact: raw.contract_impact, entrypoint: raw.entrypoint, evidence_contracts: evidenceContracts, expected_observation: raw.expected_observation, id: raw.id, kind: raw.kind, name: raw.name, producer_paths: [...raw.producer_paths].sort(cmp), requirement_refs: refs.sort((a, b) => cmp(refKey(a), refKey(b))), ...(raw.kind === 'build_or_install' ? { runnable_artifact: raw.runnable_artifact } : {}), symbol_identities: symbols, task_ids: [...raw.task_ids].sort(taskCmp), task_obligations: [...obligationByTask.values()].sort((a, b) => taskCmp(a.task_id, b.task_id)), verify_kinds: [...raw.verify_kinds].sort((a, b) => VERIFY_KINDS.indexOf(a) - VERIFY_KINDS.indexOf(b)) });
    }
    const surfaceById = new Map(normalized.map(surface => [surface.id, surface]));
    for (const surface of normalized) {
        if (surface.symbol_identities === null) continue;
        if (surface.change_kind === 'added') for (const identity of surface.symbol_identities.current) {
            const priorOwnerId = identityOwners.base.get(canonical(identity));
            if (priorOwnerId && surfaceById.get(priorOwnerId)?.change_kind !== 'removed') throw Error('added symbol identity has a non-removed base owner: ' + surface.id);
        }
        if (surface.change_kind === 'removed') for (const identity of surface.symbol_identities.base) {
            const nextOwnerId = identityOwners.current.get(canonical(identity));
            if (nextOwnerId && surfaceById.get(nextOwnerId)?.change_kind !== 'added') throw Error('removed symbol identity has a non-added current owner: ' + surface.id);
        }
    }
    normalized.sort((a, b) => cmp(a.id, b.id));
    return { discovery: { compile_commands_path: block.discovery.compile_commands_path, mode: block.discovery.mode }, schema_version: 1, surfaces: normalized };
}

function parsePlanFromChangeRoot(root, change, changeRoot) {
    if (!CHANGE_ID.test(change)) throw Error('invalid change ID');
    root = fs.realpathSync(path.resolve(root));
    changeRoot = path.resolve(changeRoot);
    const changesRoot = path.join(root, 'openspec', 'changes');
    const activeRoot = path.join(changesRoot, change);
    const archiveRoot = path.join(changesRoot, 'archive');
    const archivedName = path.basename(changeRoot);
    const active = changeRoot === activeRoot;
    const archived = path.dirname(changeRoot) === archiveRoot &&
        new RegExp('^\\d{4}-\\d{2}-\\d{2}-' + change.replace(/[.*+?^${}()|[\]\\]/g, '\\$&') + '$').test(archivedName);
    if (!active && !archived) throw Error('change root escaped active/archive roots');
    for (const dir of [path.join(root, 'openspec'), changesRoot, ...(archived ? [archiveRoot] : []), changeRoot]) {
        const st = fs.lstatSync(dir);
        if (!st.isDirectory() || st.isSymbolicLink()) throw Error('unsafe change root ancestry');
    }
    const designFile = path.join(changeRoot, 'design.md'), tasksFile = path.join(changeRoot, 'tasks.md');
    for (const file of [designFile, tasksFile]) { const st = fs.lstatSync(file); if (!st.isFile() || st.isSymbolicLink()) throw Error('unsafe planning artifact'); }
    const design = fs.readFileSync(designFile, 'utf8'), tasks = parseTasks(fs.readFileSync(tasksFile, 'utf8')), deltaUniverse = parseDeltaUniverse(changeRoot);
    const raw = extractBlock(design, '<!-- autoai:integration-completeness:v1 -->', '<!-- /autoai:integration-completeness:v1 -->', 'Integration Completeness v1');
    const block = normalizePlan(raw, tasks, deltaUniverse), blockSha = sha(Buffer.from(canonical(block)));
    return { block, block_sha256: blockSha, tasks, delta_universe: deltaUniverse, surfaces: new Map(block.surfaces.map(s => [s.id, s])) };
}

function parsePlan(root, change) {
    root = path.resolve(root);
    return parsePlanFromChangeRoot(root, change, path.join(root, 'openspec', 'changes', change));
}

function parseEconomy(designText) {
    const variants = [
        ['<!-- autoai:implementation-economy:v1 -->', '<!-- /autoai:implementation-economy:v1 -->', 'Implementation Economy v1'],
        ['<!-- autoai:implementation-economy:v2 -->', '<!-- /autoai:implementation-economy:v2 -->', 'Implementation Economy v2']
    ];
    const present = variants.filter(([start, end]) => designText.includes(start) || designText.includes(end));
    if (present.length !== 1) throw Error('exactly one supported Implementation Economy block required');
    const value = extractBlock(designText, ...present[0]);
    const expected = present[0][0].includes(':v2') ? 2 : 1;
    if (value.schema_version !== expected) throw Error('Implementation Economy marker/schema mismatch');
    return value;
}

function normalizeSurfaceRoles(plan, taskId, kind, pairs, forEvaluator = false) {
    const seen = new Set, normalized = [];
    for (const pair of pairs) {
        if (!pair || typeof pair.surface_id !== 'string' || !ROLE_ORDER.includes(pair.role)) throw Error('invalid surface role');
        const key = pair.surface_id + '\0' + pair.role; if (seen.has(key)) throw Error('duplicate surface role'); seen.add(key);
        const surface = plan.surfaces.get(pair.surface_id); if (!surface) throw Error('unknown surface ID');
        if (!forEvaluator) {
            const obligation = surface.task_obligations.find(x => x.task_id === taskId); if (!obligation || !obligation.verify_kinds.includes(kind) || !obligation.evidence_roles.includes(pair.role)) throw Error('surface role is not assigned to this task/kind');
        } else if (!surface.verify_kinds.includes(kind) || !requiredRoles(surface).includes(pair.role)) throw Error('surface role is not valid for this command kind');
        normalized.push({ surface_id: pair.surface_id, role: pair.role });
    }
    normalized.sort((a, b) => cmp(a.surface_id, b.surface_id) || ROLE_ORDER.indexOf(a.role) - ROLE_ORDER.indexOf(b.role));
    return { surface_ids: [...new Set(normalized.map(x => x.surface_id))].sort(cmp), surface_evidence_roles: normalized };
}

function bindSurfaceProbes(plan, taskId, kind, pairs, argv, expectedExitCodes, forEvaluator = false) {
    const normalized = normalizeSurfaceRoles(plan, taskId, kind, pairs, forEvaluator);
    if (!Array.isArray(argv) || !argv.length || !Array.isArray(expectedExitCodes) || !expectedExitCodes.length) throw Error('surface probe command contract is incomplete');
    const bindings = [], outputContains = [];
    for (const pair of normalized.surface_evidence_roles) {
        const surface = plan.surfaces.get(pair.surface_id), probe = surface.evidence_contracts.find(item => item.kind === kind && item.role === pair.role);
        if (!probe) throw Error('surface probe contract is missing');
        if (canonical(probe.argv) !== canonical(argv) || canonical(probe.expected_exit_codes) !== canonical(expectedExitCodes)) throw Error('surface command does not match the approved probe contract');
        bindings.push({ surface_id: pair.surface_id, role: pair.role, probe_id: probe.probe_id });
        if (!outputContains.includes(probe.output_contains)) outputContains.push(probe.output_contains);
    }
    bindings.sort((a, b) => cmp(a.surface_id, b.surface_id) || ROLE_ORDER.indexOf(a.role) - ROLE_ORDER.indexOf(b.role));
    outputContains.sort(cmp);
    return { ...normalized, surface_probe_bindings: bindings, required_output_contains: outputContains };
}

function expectedTaskSurfaceIds(plan, taskId) {
    return plan.block.surfaces.filter(s => s.task_ids.includes(taskId)).map(s => s.id).sort(cmp);
}

function verifyIntegrationEvidenceFromChangeRoot(root, change, changeRoot, options = {}) {
    root = fs.realpathSync(path.resolve(root)); changeRoot = path.resolve(changeRoot);
    const plan = parsePlanFromChangeRoot(root, change, changeRoot), base = path.join(changeRoot, 'harness');
    const staleTaskObligations = detail => {
        const error = Error('stale_task_obligations');
        error.gateStatus = 'blocked';
        error.detail = detail;
        throw error;
    };
    const verification = JSON.parse(fs.readFileSync(path.join(base, 'verification.json'))), snapshot = JSON.parse(fs.readFileSync(path.join(base, 'ai_snapshot.json')));
    closed(verification, ['schema_version', 'change_name', 'migration', 'tasks'], 'verification v3');
    if (verification.schema_version !== 3 || verification.change_name !== change || !Array.isArray(verification.tasks)) throw Error('verification v3 identity mismatch');
    const manifestPolicy = require(path.join(root, 'scripts', 'manifest_policy.js'));
    const planning=manifestPolicy.planningStateAt(root,change,changeRoot);
    if (snapshot.schema_version !== 4 || snapshot.planned_change_fingerprint!==planning.planning_fingerprint || snapshot.planned_tdd_policy_sha256!==planning.tdd_policy_sha256 || snapshot.planned_integration_completeness_sha256 !== plan.block_sha256 || planning.integration_completeness_sha256!==plan.block_sha256) throw Error('approved Integration planning baseline is missing or stale');
    const source = options.sourceFingerprint || cp.execFileSync(path.join(root, 'scripts', 'source_fingerprint.sh'), ['--kind', 'source'], { cwd: root, encoding: 'utf8' }).trim();
    if (!DIGEST.test(source)) throw Error('invalid expected source fingerprint');
    const tdd = manifestPolicy.parseTddPolicy(fs.readFileSync(path.join(changeRoot, 'design.md'), 'utf8'), fs.readFileSync(path.join(changeRoot, 'tasks.md'), 'utf8'));
    const exceptionByTask = new Map; for (const exception of tdd.policy.exceptions) for (const id of exception.task_ids) exceptionByTask.set(id, exception);
    const taskMap = new Map;
    for (const taskEvidence of verification.tasks) {
        closed(taskEvidence, ['task_id', 'requirement_refs', 'surface_ids', 'changed_paths', 'footprint_observation', 'commands'], 'verification task v3');
        if (taskMap.has(taskEvidence.task_id) || !plan.tasks.has(taskEvidence.task_id) || !Array.isArray(taskEvidence.commands)) throw Error('invalid verification task');
        exactSet(taskEvidence.surface_ids, expectedTaskSurfaceIds(plan, taskEvidence.task_id), 'verification task surface IDs');
        const taskException = exceptionByTask.get(taskEvidence.task_id);
        const unavailableException = taskException && ['unavailable_hardware', 'unavailable_external_service'].includes(taskException.category);
        for (const command of taskEvidence.commands) {
            if (!Array.isArray(command.surface_ids) || !Array.isArray(command.surface_evidence_roles) || !Array.isArray(command.surface_probe_bindings)) throw Error('command surface bindings missing');
            const normalized = normalizeSurfaceRoles(plan, taskEvidence.task_id, command.kind, command.surface_evidence_roles, false);
            if (canonical(normalized.surface_evidence_roles) !== canonical(command.surface_evidence_roles) || canonical(normalized.surface_ids) !== canonical(command.surface_ids)) throw Error('command surface bindings are not canonical');
            const closesWithProbe = command.phase === 'REGRESSION' || command.phase === 'ALTERNATIVE' && taskException && !unavailableException;
            if (closesWithProbe) {
                if (command.phase === 'ALTERNATIVE' && command.exception_id !== taskException.id) throw Error('ALTERNATIVE surface binding does not match the approved exception');
                const probes = bindSurfaceProbes(plan, taskEvidence.task_id, command.kind, command.surface_evidence_roles, command.argv, command.expected_exit_codes, false);
                if (canonical(probes.surface_probe_bindings) !== canonical(command.surface_probe_bindings)) throw Error('command surface probe bindings are not canonical');
            } else {
                if (command.phase === 'ALTERNATIVE' && (!taskException || command.exception_id !== taskException.id)) throw Error('ALTERNATIVE surface binding does not match the approved exception');
                if (command.surface_probe_bindings.length) throw Error('only REGRESSION or an approved non-environment ALTERNATIVE may carry closing surface probe bindings');
            }
        }
        taskMap.set(taskEvidence.task_id, taskEvidence);
    }
    const targetTaskIds = options.taskId ? [options.taskId] : [...plan.tasks.keys()]; const provisional = [], closures = [];
    const tddOptions = { ...(options.taskId ? { taskId: options.taskId } : {}), ...(options.requireDone ? { requireDone: true } : {}), sourceFingerprint: source };
    const tddClosure = manifestPolicy.verifyTddEvidenceAt(root, change, changeRoot, tddOptions), stillBlocked = new Set(tddClosure.blocking_exception_task_ids);
    for (const taskId of targetTaskIds) {
        const plannedTask = plan.tasks.get(taskId); if (!plannedTask) throw Error('unknown task'); if (options.requireDone && !plannedTask.done) throw Error('task is not complete');
        const evidence = taskMap.get(taskId); if (!evidence) staleTaskObligations('missing verification task: ' + taskId);
        const taskException = exceptionByTask.get(taskId), unavailable = taskException && ['unavailable_hardware', 'unavailable_external_service'].includes(taskException.category), blocked = unavailable && stillBlocked.has(taskId);
        for (const surface of plan.block.surfaces.filter(s => s.task_ids.includes(taskId))) {
            const obligation = surface.task_obligations.find(x => x.task_id === taskId); if (!obligation.verify_kinds.length) continue;
            for (const kind of obligation.verify_kinds) for (const role of obligation.evidence_roles) {
                const probeId = surface.evidence_contracts.find(item => item.kind === kind && item.role === role)?.probe_id;
                const matches = evidence.commands.filter(command => {
                    if (command.kind !== kind || !['REGRESSION', 'ALTERNATIVE'].includes(command.phase) || command.result !== 'Pass' || command.source_fingerprint_after !== source || command.planning_fingerprint_after!==planning.planning_fingerprint || command.tdd_policy_sha256!==planning.tdd_policy_sha256 || !command.surface_evidence_roles.some(x => x.surface_id === surface.id && x.role === role)) return false;
                    if (command.phase === 'REGRESSION') return command.surface_probe_bindings.some(x => x.surface_id === surface.id && x.role === role && x.probe_id === probeId);
                    if (!taskException || command.exception_id !== taskException.id) return false;
                    if (unavailable) return blocked && !command.surface_probe_bindings.length;
                    return command.surface_probe_bindings.some(x => x.surface_id === surface.id && x.role === role && x.probe_id === probeId);
                });
                if (!matches.length) staleTaskObligations(`missing current surface closure: ${surface.id}/${taskId}/${kind}/${role}`);
                const command = blocked
                    ? matches.filter(item => item.phase === 'ALTERNATIVE' && item.exception_id === taskException.id).at(-1)
                    : taskException && !unavailable
                        ? matches.filter(item => item.phase === 'ALTERNATIVE' && item.exception_id === taskException.id).at(-1)
                        : matches.findLast(item => item.phase === 'REGRESSION');
                if (!command) staleTaskObligations(`missing ${blocked?'provisional ALTERNATIVE':taskException&&!unavailable?'approved ALTERNATIVE':'recovered REGRESSION'} surface closure: ${surface.id}/${taskId}/${kind}/${role}`);
                if (blocked) provisional.push({ surface_id: surface.id, task_id: taskId, kind, role, exception_id: taskException.id });
                else closures.push({ surface_id: surface.id, task_id: taskId, kind, role, command_id: command.id });
            }
        }
    }
    return { source_fingerprint: source, planning_block_sha256: plan.block_sha256, verified: closures, provisionally_blocked: provisional };
}

function verifyIntegrationEvidence(root, change, options = {}) {
    root = path.resolve(root);
    return verifyIntegrationEvidenceFromChangeRoot(root, change, path.join(root, 'openspec', 'changes', change), options);
}

function currentRegularFile(root, p) {
    safePath(p, 'current file');
    const anchor = fs.realpathSync(path.resolve(root)), prefix = anchor.endsWith(path.sep) ? anchor : anchor + path.sep;
    let cursor = anchor;
    try {
        const parts = p.split('/');
        for (let index = 0; index < parts.length; index++) {
            cursor = path.join(cursor, parts[index]); const st = fs.lstatSync(cursor);
            if (st.isSymbolicLink() || index < parts.length - 1 && !st.isDirectory() || index === parts.length - 1 && !st.isFile()) return false;
        }
        const resolved = fs.realpathSync(cursor);
        return resolved.startsWith(prefix) && resolved === cursor;
    } catch (error) {
        if (['ENOENT', 'ENOTDIR', 'ELOOP'].includes(error.code)) return false;
        throw error;
    }
}
function baseFile(root, base, p) { try { const out = cp.execFileSync('git', ['ls-tree', '-z', base, '--', p], { cwd: root }).toString('utf8'); return /^(100644|100755) blob [0-9a-f]+\t/.test(out); } catch { return false; } }

function validateAstIdentityInventory(plan, astResult) {
    const inventory = astResult?.identity_inventory;
    if (!inventory) throw Object.assign(Error('ast_identity_inventory_missing'), { gateStatus: 'invalid' });
    closed(inventory, ['base', 'current'], 'AST identity inventory');
    const sets = {}, owners = { base: new Map, current: new Map };
    for (const side of ['base', 'current']) {
        if (!Array.isArray(inventory[side])) throw Object.assign(Error('ast_identity_inventory_invalid'), { gateStatus: 'invalid' });
        const normalized = inventory[side].map(identity => normalizeSymbolIdentity(identity)), keys = normalized.map(canonical);
        if (keys.length !== new Set(keys).size) throw Object.assign(Error('ast_identity_inventory_collision'), { gateStatus: 'invalid' });
        sets[side] = new Set(keys);
    }
    for (const surface of plan.block.surfaces) {
        if (surface.symbol_identities === null) continue;
        for (const side of ['base', 'current']) for (const identity of surface.symbol_identities[side]) owners[side].set(canonical(identity), surface);
    }
    for (const surface of plan.block.surfaces) {
        const symbols = surface.symbol_identities; if (symbols === null) continue;
        for (const side of ['base', 'current']) for (const identity of symbols[side]) {
            if (!surface.producer_paths.includes(identity.declaration_path)) throw Object.assign(Error('planned_symbol_declaration_is_not_a_producer:' + surface.id), { gateStatus: 'invalid' });
            if (!sets[side].has(canonical(identity))) throw Object.assign(Error('planned_symbol_identity_missing_from_' + side + ':' + surface.id), { gateStatus: 'invalid' });
        }
        if (surface.change_kind === 'added' && symbols.current.some(identity => {
            const key = canonical(identity), priorOwner = owners.base.get(key);
            return sets.base.has(key) && (!priorOwner || priorOwner.change_kind !== 'removed');
        })) throw Object.assign(Error('added_symbol_already_exists_in_base:' + surface.id), { gateStatus: 'invalid' });
        if (surface.change_kind === 'removed' && symbols.base.some(identity => {
            const key = canonical(identity), nextOwner = owners.current.get(key);
            return sets.current.has(key) && (!nextOwner || nextOwner.change_kind !== 'added');
        })) throw Object.assign(Error('removed_symbol_still_exists_in_current:' + surface.id), { gateStatus: 'invalid' });
        if (['modified', 'deprecated'].includes(surface.change_kind) && (!symbols.base.length || !symbols.current.length)) throw Object.assign(Error('changed_symbol_requires_both_tree_sides:' + surface.id), { gateStatus: 'invalid' });
    }
}

function buildReviewedReport(root, change, instructionsFile) {
    root = path.resolve(root); const plan = parsePlan(root, change), changeRoot = path.join(root, 'openspec', 'changes', change), harness = path.join(changeRoot, 'harness');
    const snapshot = JSON.parse(fs.readFileSync(path.join(harness, 'ai_snapshot.json'))), instructions = JSON.parse(fs.readFileSync(instructionsFile));
    if (snapshot.schema_version !== 4 || snapshot.planned_integration_completeness_sha256 !== plan.block_sha256 || !/^[0-9a-f]{40,64}$/.test(snapshot.implementation_base_commit || '')) throw Object.assign(Error('planning_or_implementation_baseline'), { gateStatus: 'blocked' });
    const progress = instructions?.progress;
    if (instructions?.changeName !== change || instructions?.schemaName !== 'spec-driven' || instructions?.state !== 'all_done' || !progress || progress.total <= 0 || progress.remaining !== 0 || progress.complete !== progress.total) throw Object.assign(Error('incomplete_tasks'), { gateStatus: 'blocked' });
    const closure = verifyIntegrationEvidence(root, change, { requireDone: true });
    const design = fs.readFileSync(path.join(changeRoot, 'design.md'), 'utf8'), economy = parseEconomy(design), scope = scopeLib.collectScope(root, snapshot.implementation_base_commit, economy.classification, economy.schema_version);
    if (scope.unclassified_paths.length || scope.classification_overlaps.length) throw Object.assign(Error('invalid_scope_classification'), { gateStatus: 'invalid' });
    const footprintFile = path.join(harness, 'change-footprint.json'), footprintRaw = fs.readFileSync(footprintFile), footprint = JSON.parse(footprintRaw);
    if (footprint.implementation_base_commit !== snapshot.implementation_base_commit || footprint.source_fingerprint !== closure.source_fingerprint) throw Object.assign(Error('stale_change_footprint'), { gateStatus: 'blocked' });
    const helperFile = path.join(root, 'scripts', 'change_scope.js'), adapterFile = path.join(root, 'scripts', 'integration_surface_lib.js');
    const helperIdentity = { schema_version: 1, path: 'scripts/change_scope.js', sha256: sha(fs.readFileSync(helperFile)), output_sha256: scope.output_sha256 };
    let astResult = { candidates: [], compile_commands_sha256: null, ast_tool_identity: null, adapter_identity: { id: 'reviewed-inventory-v1', schema_version: 1, sha256: sha(fs.readFileSync(adapterFile)) } };
    if (plan.block.discovery.mode === 'clang_ast') {
        const adapter = require(path.join(root, 'scripts', 'clang_ast_surface_adapter.js'));
        astResult = adapter.discover({ root, change, implementationBase: snapshot.implementation_base_commit, compileCommandsPath: plan.block.discovery.compile_commands_path, plan, scope, classification: economy.classification });
        validateAstIdentityInventory(plan, astResult);
    }
    const classify = scopeLib.compileClassification(economy.classification), classesFor = p => classify.classes.filter(k => classify.patterns[k].some(re => re.test(p)));
    const allowedConsumerClasses = {
        production_caller: ['production'], representative_external: ['tests', 'examples', 'project_tooling'], registration_dispatch: ['production', 'tests'],
        real_entrypoint: ['production', 'tests', 'examples', 'project_tooling'], producer_consumer_pair: ['production', 'tests', 'examples'], downstream_build: ['tests', 'examples', 'project_tooling'], compatibility_probe: ['tests', 'examples', 'project_tooling']
    };
    for (const surface of plan.block.surfaces) {
        for (const p of surface.producer_paths) {
            const now = currentRegularFile(root, p), before = baseFile(root, snapshot.implementation_base_commit, p);
            if (surface.change_kind === 'added' ? !now : surface.change_kind === 'removed' ? !before : !now && !before) throw Object.assign(Error('producer_path_state:' + p), { gateStatus: 'invalid' });
        }
        for (const p of [...surface.consumer_paths, ...(surface.compatibility?.old_consumer_paths || []), ...(surface.compatibility?.replacement_consumer_paths || [])]) {
            if (!currentRegularFile(root, p)) throw Object.assign(Error('current_consumer_path_missing:' + p), { gateStatus: 'invalid' });
            const consumerClasses = classesFor(p);
            if (consumerClasses.length !== 1 || !allowedConsumerClasses[surface.consumer_kind].includes(consumerClasses[0])) throw Object.assign(Error('consumer_path_classification:' + p), { gateStatus: 'invalid' });
        }
    }
    const pathCandidates = scope.logical_changes.filter(x => x.classifications.includes('production')).map(changeRecord => {
        const id = crypto.createHash('sha256').update('production-path\0' + changeRecord.change_status + '\0' + (changeRecord.old_path || '') + '\0' + changeRecord.path).digest('hex').slice(0, 16);
        return { candidate_id: 'path-candidate-' + id, source: 'path', path: changeRecord.path, old_path: changeRecord.old_path, change_status: changeRecord.change_status };
    }).sort((a, b) => cmp(a.candidate_id, b.candidate_id));
    const structuralCandidates = scope.structural_candidates.map(x => ({ candidate_id: x.candidate_id, source: 'structural', path: x.path, kind: x.kind, allowance_kind: x.allowance_kind, old_path: x.old_path, change_status: x.change_status })).sort((a, b) => cmp(a.candidate_id, b.candidate_id));
    const allCandidates = [...pathCandidates, ...structuralCandidates, ...astResult.candidates]; const candidateIds = new Set;
    for (const candidate of allCandidates) { if (candidateIds.has(candidate.candidate_id)) throw Object.assign(Error('candidate_id_collision'), { gateStatus: 'invalid' }); candidateIds.add(candidate.candidate_id); }
    const bindings = [], mapped = new Map;
    for (const surface of plan.block.surfaces) {
        const candidateBindings = [];
        for (const candidate of [...pathCandidates, ...structuralCandidates]) {
            const candidatePaths = [candidate.path, ...(candidate.old_path ? [candidate.old_path] : [])];
            const roles = [];
            if (candidatePaths.some(p => surface.producer_paths.includes(p))) roles.push('producer');
            if (candidatePaths.some(p => [...surface.consumer_paths, ...(surface.compatibility?.old_consumer_paths || []), ...(surface.compatibility?.replacement_consumer_paths || [])].includes(p))) roles.push('consumer');
            for (const role of roles) {
                let side = candidate.change_status === 'added' ? 'current' : candidate.change_status === 'deleted' ? 'base' : 'both';
                if (role === 'producer') side = surface.change_kind === 'added' ? 'current' : surface.change_kind === 'removed' ? 'base' : side;
                candidateBindings.push({ candidate_id: candidate.candidate_id, role, tree_side: side });
                if (!mapped.has(candidate.candidate_id)) mapped.set(candidate.candidate_id, []); mapped.get(candidate.candidate_id).push({ surface_id: surface.id, role });
            }
        }
        for (const candidate of astResult.candidates) {
            const symbols = surface.symbol_identities;
            if (!symbols) continue;
            const baseMatch = candidate.base_symbol_identity !== null && symbols.base.some(x => canonical(x) === canonical(candidate.base_symbol_identity));
            const currentMatch = candidate.current_symbol_identity !== null && symbols.current.some(x => canonical(x) === canonical(candidate.current_symbol_identity));
            const side = surface.change_kind === 'added' ? 'current' : surface.change_kind === 'removed' ? 'base' : 'both';
            const matched = side === 'current' ? currentMatch : side === 'base' ? baseMatch : baseMatch && currentMatch;
            if (matched) {
                candidateBindings.push({ candidate_id: candidate.candidate_id, role: 'producer', tree_side: side });
                if (!mapped.has(candidate.candidate_id)) mapped.set(candidate.candidate_id, []);
                mapped.get(candidate.candidate_id).push({ surface_id: surface.id, role: 'producer' });
            }
        }
        candidateBindings.sort((a, b) => cmp(a.candidate_id + a.role + a.tree_side, b.candidate_id + b.role + b.tree_side));
        if (!candidateBindings.some(x => x.role === 'producer')) throw Object.assign(Error('surface_has_no_changed_producer:' + surface.id), { gateStatus: 'invalid' });
        const producerSides=new Set(candidateBindings.filter(x=>x.role==='producer').flatMap(x=>x.tree_side==='both'?['base','current']:[x.tree_side])),requiredSides=surface.change_kind==='added'?['current']:surface.change_kind==='removed'?['base']:['base','current'];if(requiredSides.some(side=>!producerSides.has(side)))throw Object.assign(Error('surface_producer_tree_side:'+surface.id),{gateStatus:'invalid'});
        bindings.push({ surface_id: surface.id, candidate_bindings: candidateBindings, producer_paths: surface.producer_paths, consumer_paths: surface.consumer_paths, old_consumer_paths: surface.compatibility?.old_consumer_paths || [], replacement_consumer_paths: surface.compatibility?.replacement_consumer_paths || [] });
    }
    const unmatched = allCandidates.filter(x => !mapped.has(x.candidate_id)).map(x => ({ candidate_id: x.candidate_id, source: x.source, reason: x.source === 'clang_ast' ? 'No exact approved symbol identity matched this declaration candidate.' : 'The changed path is not mapped to an approved product surface.' })).sort((a, b) => cmp(a.candidate_id, b.candidate_id));
    const blockingUnmatched = unmatched.some(x =>
        x.source === 'clang_ast' &&
        astResult.candidates.find(c => c.candidate_id === x.candidate_id)?.candidate_scope === 'public_contract'
    );
    const status = blockingUnmatched ? 'orphaned' : unmatched.length ? 'review_required' : 'complete';
    return {
        schema_version: 1, change_name: change, implementation_base_commit: snapshot.implementation_base_commit, discovery_mode: plan.block.discovery.mode,
        source_fingerprint: closure.source_fingerprint, planning_block_sha256: plan.block_sha256, change_footprint_json_sha256: sha(footprintRaw),
        scope_classifier_identity: helperIdentity, discovery_adapter_identity: astResult.adapter_identity,
        compile_commands_sha256: astResult.compile_commands_sha256, ast_tool_identity: astResult.ast_tool_identity,
        planned_surface_ids: plan.block.surfaces.map(x => x.id), changed_production_paths: scope.changed_production_paths,
        path_candidates: pathCandidates, structural_candidates: structuralCandidates, ast_candidates: astResult.candidates,
        surface_candidate_bindings: bindings.sort((a, b) => cmp(a.surface_id, b.surface_id)), unmatched_candidates: unmatched, status
    };
}

function discoveryIdentity(report) {
    return sha(Buffer.from(canonical({ discovery_mode: report.discovery_mode, scope_classifier_identity: report.scope_classifier_identity, discovery_adapter_identity: report.discovery_adapter_identity, compile_commands_sha256: report.compile_commands_sha256, ast_tool_identity: report.ast_tool_identity })));
}

/*
 * Validate only the Integration Completeness portion of an Evaluation v3.
 * The caller remains responsible for the existing behavioral, review and
 * Implementation Economy contracts.  This function is deliberately free of
 * writes so finish, recheck, archive and focused tests can share one verdict.
 */
function validateEvaluationV3(context) {
    const root = path.resolve(context.root || process.cwd()), change = context.change;
    const baseline = context.baseline, evaluation = context.evaluation, ledger = context.ledger, report = context.report;
    const plan = context.plan || parsePlan(root, change), directClosure = context.direct_closure || (context.plan ? null : verifyIntegrationEvidence(root, change, { requireDone: true }));
    const fail = message => { throw Error('Evaluation v3 Integration Completeness: ' + message); };
    let evaluationClassification = context.classification || null;
    if (!evaluationClassification && !context.plan) {
        try {
            evaluationClassification = parseEconomy(fs.readFileSync(path.join(root, 'openspec', 'changes', change, 'design.md'), 'utf8')).classification;
        } catch (error) {
            fail('cannot load shared path classification: ' + error.message);
        }
    }
    if (plan.block.discovery.mode === 'clang_ast' && !evaluationClassification) fail('shared path classification is required for AST validation');
    let isProduction = context.is_production || (() => true), contractPathPolicy;
    if (evaluationClassification) {
        try {
            const compiled = scopeLib.compileClassification(evaluationClassification);
            if (!context.is_production) isProduction = value => compiled.patterns.production.some(pattern => pattern.test(value));
            contractPathPolicy = scopeLib.compileContractPathPolicy(evaluationClassification);
        } catch (error) {
            fail('shared path classification is invalid: ' + error.message);
        }
    } else {
        contractPathPolicy = value => isProduction(value) && scopeLib.contractPathSyntax(value);
    }
    const set = (values, label, order = cmp) => {
        if (!Array.isArray(values) || values.length !== new Set(values.map(canonical)).size) fail(label + ' must be a unique array');
        if (values.some((value, index) => index && order(values[index - 1], value) > 0)) fail(label + ' must use canonical order');
        return values;
    };
    const sameSet = (actual, expected, label, order = cmp) => {
        set(actual, label, order); set(expected, label + ' expected', order);
        if (canonical(actual) !== canonical(expected)) fail(label + ' coverage mismatch');
    };
    const ids = (values, label) => set(values, label).map(value => nonempty(value, label));
    const result = (value, label) => { if (!['Pass', 'Fail', 'Blocked'].includes(value)) fail(label + ' result invalid'); return value; };
    const digest = (value, label) => { if (!DIGEST.test(value || '')) fail(label + ' digest invalid'); return value; };
    const object = (value, fields, label) => { try { closed(value, fields, label); } catch (error) { fail(error.message); } return value; };
    const paths = (values, label) => {
        set(values, label);
        for (const value of values) { try { safePath(value, label); } catch (error) { fail(error.message); } }
        return values;
    };
    const exactPaths = (actual, expected, label) => sameSet(paths(actual, label), [...expected].sort(cmp), label);
    const candidateOrder = (a, b) => cmp(a.candidate_id, b.candidate_id);
    const assessmentOrder = (a, b) => cmp(a.candidate_id || a.surface_id || a.id, b.candidate_id || b.surface_id || b.id);

    if (!CHANGE_ID.test(change || '') || !baseline || !evaluation || !ledger || !report) fail('missing validator context');
    const baselineCore = ['schema_version', 'evaluation_id', 'change_name', 'status', 'started_at', 'source_fingerprint', 'artifact_fingerprint', 'base_specs_fingerprint', 'verification_json_sha256', 'budget_block_sha256', 'change_footprint_json_sha256', 'review_input', 'integration_planning_block_sha256', 'integration_surface_report_sha256', 'integration_discovery_identity_sha256'];
    const baselineTail = baseline.status === 'complete' ? ['completed_at', 'evaluation_json_sha256'] : baseline.status === 'aborted' ? ['aborted_at', 'reason'] : baseline.status === 'in_progress' ? [] : null;
    if (!baselineTail) fail('baseline status invalid');
    object(baseline, [...baselineCore, ...baselineTail], 'evaluation baseline v3');
    if (baseline.schema_version !== 3 || baseline.change_name !== change || typeof baseline.evaluation_id !== 'string') fail('baseline identity mismatch');
    if (baseline.status === 'aborted') fail('an aborted baseline cannot carry an Evaluation');
    for (const field of ['source_fingerprint', 'artifact_fingerprint', 'base_specs_fingerprint', 'verification_json_sha256', 'budget_block_sha256', 'change_footprint_json_sha256', 'integration_planning_block_sha256', 'integration_surface_report_sha256', 'integration_discovery_identity_sha256']) digest(baseline[field], field);

    const evaluationFields = ['schema_version', 'evaluation_id', 'change_name', 'verdict', 'evaluation_started_at', 'evaluated_at', 'openspec_version', 'evaluator_role', 'input_source_fingerprint', 'input_artifact_fingerprint', 'input_base_specs_fingerprint', 'source_fingerprint', 'artifact_fingerprint', 'base_specs_fingerprint', 'budget_block_sha256', 'change_footprint_json_sha256', 'review_input', 'change_review', 'implementation_economy', 'criteria', 'commands', 'blocking_untested', 'residual_risks', 'integration_completeness'];
    object(evaluation, evaluationFields, 'evaluation v3');
    if (evaluation.schema_version !== 3 || evaluation.evaluation_id !== baseline.evaluation_id || evaluation.change_name !== change || evaluation.evaluation_started_at !== baseline.started_at) fail('evaluation identity mismatch');
    result(evaluation.verdict, 'evaluation verdict');
    if (!Array.isArray(evaluation.commands) || !Array.isArray(evaluation.criteria) || !Array.isArray(evaluation.blocking_untested) || !Array.isArray(evaluation.residual_risks)) fail('evaluation arrays invalid');

    object(ledger, ['schema_version', 'evaluation_id', 'change_name', 'commands'], 'Evaluation command ledger v2');
    if (ledger.schema_version !== 2 || ledger.evaluation_id !== evaluation.evaluation_id || ledger.change_name !== change || !Array.isArray(ledger.commands) || canonical(ledger.commands) !== canonical(evaluation.commands)) fail('ledger/evaluation command identity mismatch');
    const commandFields = ['id', 'kind', 'argv', 'command', 'working_directory', 'started_at', 'finished_at', 'expected_exit_codes', 'exit_code', 'expected', 'observed', 'result', 'output_sha256', 'surface_ids', 'surface_evidence_roles', 'surface_probe_bindings'];
    const commands = new Map;
    for (const command of ledger.commands) {
        object(command, commandFields, 'Evaluation command v2');
        if (typeof command.id !== 'string' || !command.id || commands.has(command.id) || !VERIFY_KINDS.includes(command.kind) || !['Pass', 'Fail'].includes(command.result) || !Array.isArray(command.argv) || !command.argv.length || !Array.isArray(command.expected_exit_codes) || !command.expected_exit_codes.length || !command.expected_exit_codes.every(Number.isInteger) || !Number.isInteger(command.exit_code) || command.result !== (command.expected_exit_codes.includes(command.exit_code) ? 'Pass' : 'Fail')) fail('Evaluation command contract mismatch');
        digest(command.output_sha256, 'command output');
        const normalized = bindSurfaceProbes(plan, null, command.kind, command.surface_evidence_roles, command.argv, command.expected_exit_codes, true);
        if (canonical(normalized.surface_ids) !== canonical(command.surface_ids) || canonical(normalized.surface_evidence_roles) !== canonical(command.surface_evidence_roles) || canonical(normalized.surface_probe_bindings) !== canonical(command.surface_probe_bindings)) fail('command surface bindings are noncanonical or do not match the approved probe contract');
        commands.set(command.id, command);
    }

    const reportFields = ['schema_version', 'change_name', 'implementation_base_commit', 'discovery_mode', 'source_fingerprint', 'planning_block_sha256', 'change_footprint_json_sha256', 'scope_classifier_identity', 'discovery_adapter_identity', 'compile_commands_sha256', 'ast_tool_identity', 'planned_surface_ids', 'changed_production_paths', 'path_candidates', 'structural_candidates', 'ast_candidates', 'surface_candidate_bindings', 'unmatched_candidates', 'status'];
    object(report, reportFields, 'integration surface report v1');
    if (report.schema_version !== 1 || report.change_name !== change || !['complete', 'review_required', 'orphaned'].includes(report.status) || report.discovery_mode !== plan.block.discovery.mode || report.planning_block_sha256 !== plan.block_sha256) fail('surface report identity or state mismatch');
    digest(report.source_fingerprint, 'report source fingerprint'); digest(report.change_footprint_json_sha256, 'report footprint');
    if (!/^[0-9a-f]{40,64}$/.test(report.implementation_base_commit || '') || report.source_fingerprint !== baseline.source_fingerprint || report.change_footprint_json_sha256 !== baseline.change_footprint_json_sha256) fail('surface report freshness identity drifted');
    object(report.scope_classifier_identity, ['schema_version', 'path', 'sha256', 'output_sha256'], 'scope classifier identity');
    if (report.scope_classifier_identity.schema_version !== 1) fail('scope classifier identity version invalid'); safePath(report.scope_classifier_identity.path, 'scope classifier path'); digest(report.scope_classifier_identity.sha256, 'scope classifier'); digest(report.scope_classifier_identity.output_sha256, 'scope output');
    object(report.discovery_adapter_identity, ['id', 'schema_version', 'sha256'], 'discovery adapter identity'); nonempty(report.discovery_adapter_identity.id, 'discovery adapter ID'); if (report.discovery_adapter_identity.schema_version !== 1) fail('discovery adapter identity version invalid'); digest(report.discovery_adapter_identity.sha256, 'discovery adapter');
    if (report.discovery_mode === 'reviewed_inventory') {
        if (report.compile_commands_sha256 !== null || report.ast_tool_identity !== null || report.ast_candidates.length) fail('reviewed inventory carries AST identity');
    } else {
        digest(report.compile_commands_sha256, 'compile commands'); object(report.ast_tool_identity, ['resolved_path', 'version_sha256', 'capability_probe_sha256'], 'AST tool identity');
        if (typeof report.ast_tool_identity.resolved_path !== 'string' || !path.isAbsolute(report.ast_tool_identity.resolved_path)) fail('AST tool path must be absolute'); digest(report.ast_tool_identity.version_sha256, 'AST version'); digest(report.ast_tool_identity.capability_probe_sha256, 'AST capability probe');
    }
    sameSet(report.planned_surface_ids, plan.block.surfaces.map(surface => surface.id), 'report planned surface IDs');
    paths(report.changed_production_paths, 'changed production paths');
    if (!Array.isArray(report.path_candidates) || !Array.isArray(report.structural_candidates) || !Array.isArray(report.ast_candidates) || !Array.isArray(report.surface_candidate_bindings) || !Array.isArray(report.unmatched_candidates)) fail('surface report arrays invalid');
    set(report.path_candidates, 'path candidates', candidateOrder); set(report.structural_candidates, 'structural candidates', candidateOrder); set(report.ast_candidates, 'AST candidates', candidateOrder);
    set(report.surface_candidate_bindings, 'surface candidate bindings', (a, b) => cmp(a.surface_id, b.surface_id)); set(report.unmatched_candidates, 'unmatched candidates', candidateOrder);
    const candidates = new Map, candidatePaths = candidate => {
        if (candidate.source === 'clang_ast') return [...new Set([candidate.base_symbol_identity?.declaration_path, candidate.current_symbol_identity?.declaration_path].filter(Boolean))].sort(cmp);
        return [...new Set([candidate.old_path, candidate.path].filter(Boolean))].sort(cmp);
    };
    const surfaceBearingPath = p =>
        /(^|\/)include\//.test(p) ||
        /\.(?:h|hh|hpp|hxx|inc|ipp|tpp|proto|fbs|thrift|avsc|sql|json|ya?ml|toml|ini|cfg)$/i.test(p) ||
        /(^|\/)(?:config|configs|schema|schemas|protocol|protocols|plugin|plugins|registry|registries)(\/|$)/i.test(p);
    const canBePrivateImplementation = candidate => {
        if (candidate.source === 'structural' || candidatePaths(candidate).some(pathValue => isProduction(pathValue) && surfaceBearingPath(pathValue))) return false;
        if (candidate.source !== 'clang_ast') return true;
        if (candidatePaths(candidate).every(pathValue => !isProduction(pathValue))) return true;
        const sides = [
            [candidate.base_symbol_identity, candidate.base_access, candidate.base_linkage],
            [candidate.current_symbol_identity, candidate.current_access, candidate.current_linkage]
        ].filter(([identity]) => identity !== null);
        return sides.length > 0 && sides.every(([, access, linkage]) => access === 'private' || linkage === 'internal');
    };
    for (const candidate of [...report.path_candidates, ...report.structural_candidates, ...report.ast_candidates]) {
        const fields = candidate.source === 'path' ? ['candidate_id', 'source', 'path', 'old_path', 'change_status'] : candidate.source === 'structural' ? ['candidate_id', 'source', 'path', 'kind', 'allowance_kind', 'old_path', 'change_status'] : candidate.source === 'clang_ast' ? ['candidate_id', 'source', 'base_symbol_identity', 'current_symbol_identity', 'candidate_scope', 'base_access', 'current_access', 'base_linkage', 'current_linkage', 'change_status', 'base_semantic_sha256', 'current_semantic_sha256'] : null;
        if (!fields) fail('candidate source invalid'); object(candidate, fields, 'surface candidate');
        if (typeof candidate.candidate_id !== 'string' || !candidate.candidate_id || candidates.has(candidate.candidate_id)) fail('candidate ID invalid or duplicate');
        if (candidate.source !== 'clang_ast') {
            safePath(candidate.path, 'candidate path'); if (candidate.old_path !== null) safePath(candidate.old_path, 'candidate old path');
            if (!['added', 'modified', 'deleted', 'renamed'].includes(candidate.change_status) || (candidate.change_status === 'renamed') !== (candidate.old_path !== null)) fail('coarse candidate state invalid');
        } else {
            if (!['added', 'modified', 'deleted'].includes(candidate.change_status) || !['public_contract', 'reviewable'].includes(candidate.candidate_scope)) fail('AST candidate state invalid');
            if (candidate.change_status === 'added' ? candidate.base_symbol_identity !== null || candidate.current_symbol_identity === null : candidate.change_status === 'deleted' ? candidate.base_symbol_identity === null || candidate.current_symbol_identity !== null : candidate.base_symbol_identity === null || candidate.current_symbol_identity === null) fail('AST candidate tree sides invalid');
            for (const identity of [candidate.base_symbol_identity, candidate.current_symbol_identity]) if (identity !== null) { try { normalizeSymbolIdentity(identity); } catch (error) { fail(error.message); } }
            for (const access of [candidate.base_access, candidate.current_access]) if (access !== null && !['public', 'protected', 'private', 'none'].includes(access)) fail('AST access invalid');
            for (const linkage of [candidate.base_linkage, candidate.current_linkage]) if (linkage !== null && !['internal', 'external', 'module', 'unknown'].includes(linkage)) fail('AST linkage invalid');
            for (const semantic of [candidate.base_semantic_sha256, candidate.current_semantic_sha256]) if (semantic !== null) digest(semantic, 'AST semantic');
            const sideMetadata = (identity, access, linkage, semantic) => identity === null ? access === null && linkage === null && semantic === null : access !== null && linkage !== null && semantic !== null;
            if (!sideMetadata(candidate.base_symbol_identity, candidate.base_access, candidate.base_linkage, candidate.base_semantic_sha256) || !sideMetadata(candidate.current_symbol_identity, candidate.current_access, candidate.current_linkage, candidate.current_semantic_sha256)) fail('AST side metadata mismatch');
            if(candidate.change_status==='modified'&&(canonical(candidate.base_symbol_identity)!==canonical(candidate.current_symbol_identity)||candidate.base_semantic_sha256===candidate.current_semantic_sha256&&candidate.base_access===candidate.current_access&&candidate.base_linkage===candidate.current_linkage))fail('AST modified candidate identity/semantic mismatch');const expectedId='clang-ast-'+crypto.createHash('sha256').update('clang-ast-v1'+candidate.change_status+canonical(candidate.base_symbol_identity)+canonical(candidate.current_symbol_identity)).digest('hex').slice(0,16);if(candidate.candidate_id!==expectedId)fail('AST candidate ID mismatch');let publicContract;try{publicContract=[[candidate.base_symbol_identity,candidate.base_access,candidate.base_linkage],[candidate.current_symbol_identity,candidate.current_access,candidate.current_linkage]].some(([identity,access,linkage])=>identity!==null&&access!=='private'&&linkage!=='internal'&&contractPathPolicy(identity.declaration_path))}catch(error){fail('AST contract-path classification failed: '+error.message)}if(candidate.candidate_scope!==(publicContract?'public_contract':'reviewable'))fail('AST candidate scope mismatch');
        }
        candidates.set(candidate.candidate_id, candidate);
    }
    const typed = new Map;
    for (const row of report.surface_candidate_bindings) {
        object(row, ['surface_id', 'candidate_bindings', 'producer_paths', 'consumer_paths', 'old_consumer_paths', 'replacement_consumer_paths'], 'surface candidate binding');
        const surface = plan.surfaces.get(row.surface_id); if (!surface || !Array.isArray(row.candidate_bindings)) fail('unknown report surface binding');
        exactPaths(row.producer_paths, surface.producer_paths, 'binding producer paths'); exactPaths(row.consumer_paths, surface.consumer_paths, 'binding consumer paths');
        exactPaths(row.old_consumer_paths, surface.compatibility?.old_consumer_paths || [], 'binding old consumer paths'); exactPaths(row.replacement_consumer_paths, surface.compatibility?.replacement_consumer_paths || [], 'binding replacement consumer paths');
        set(row.candidate_bindings, 'typed candidate bindings', (a, b) => cmp(a.candidate_id + a.role + a.tree_side, b.candidate_id + b.role + b.tree_side));
        let producers = 0;
        for (const binding of row.candidate_bindings) {
            object(binding, ['candidate_id', 'role', 'tree_side'], 'typed candidate binding');
            if (!candidates.has(binding.candidate_id) || !['producer', 'consumer'].includes(binding.role) || !['base', 'current', 'both'].includes(binding.tree_side)) fail('typed candidate binding invalid');
            if (binding.role === 'producer') producers++;
            const list = typed.get(binding.candidate_id) || []; list.push({ surface_id: row.surface_id, role: binding.role, tree_side: binding.tree_side }); typed.set(binding.candidate_id, list);
        }
        if (!producers) fail('surface has no producer candidate: ' + row.surface_id);
        const producerSides=new Set(row.candidate_bindings.filter(binding=>binding.role==='producer').flatMap(binding=>binding.tree_side==='both'?['base','current']:[binding.tree_side])),requiredSides=surface.change_kind==='added'?['current']:surface.change_kind==='removed'?['base']:['base','current'];if(requiredSides.some(side=>!producerSides.has(side)))fail('surface producer tree-side coverage mismatch: '+row.surface_id);
    }
    sameSet(report.surface_candidate_bindings.map(row => row.surface_id), plan.block.surfaces.map(surface => surface.id), 'report surface bindings');
    const unmatched = new Map;
    for (const item of report.unmatched_candidates) {
        object(item, ['candidate_id', 'source', 'reason'], 'unmatched candidate');
        if (!candidates.has(item.candidate_id) || candidates.get(item.candidate_id).source !== item.source || typed.has(item.candidate_id) || unmatched.has(item.candidate_id)) fail('unmatched candidate identity invalid');
        nonempty(item.reason, 'unmatched candidate reason'); unmatched.set(item.candidate_id, item);
    }
    sameSet([...new Set([...typed.keys(), ...unmatched.keys()])].sort(cmp), [...candidates.keys()].sort(cmp), 'candidate partition');
    if ([...typed.keys()].some(id => unmatched.has(id))) fail('candidate partition overlaps');
    const blockingUnmatched = [...unmatched].some(([id]) =>
        candidates.get(id).source === 'clang_ast' &&
        candidates.get(id).candidate_scope === 'public_contract'
    );
    const expectedReportStatus = blockingUnmatched ? 'orphaned' : unmatched.size ? 'review_required' : 'complete';
    if (report.status !== expectedReportStatus) fail('report status does not match candidate partition');
    if (report.status === 'orphaned') fail('orphaned reports are Planner diagnostics and cannot establish an Evaluation attempt');

    const raw = context.report_raw === undefined ? reportBytes(report) : Buffer.isBuffer(context.report_raw) ? context.report_raw : Buffer.from(context.report_raw);
    if (!raw.equals(reportBytes(report))) fail('surface report bytes are noncanonical');
    const reportSha = sha(raw), discoverySha = discoveryIdentity(report);
    if (baseline.integration_planning_block_sha256 !== plan.block_sha256 || baseline.integration_surface_report_sha256 !== reportSha || baseline.integration_discovery_identity_sha256 !== discoverySha) fail('baseline Integration digest drift');

    const integration = evaluation.integration_completeness;
    object(integration, ['planning_block_sha256', 'report_sha256', 'discovery_identity_sha256', 'inventory_assessment', 'candidate_assessments', 'surface_assessments', 'orphan_surfaces', 'result'], 'integration_completeness');
    if (integration.planning_block_sha256 !== baseline.integration_planning_block_sha256 || integration.report_sha256 !== baseline.integration_surface_report_sha256 || integration.discovery_identity_sha256 !== baseline.integration_discovery_identity_sha256) fail('Evaluation Integration digest mirrors drifted');
    const reviewInput = evaluation.review_input;
    if (!reviewInput || !Array.isArray(reviewInput.review_paths)) fail('review input paths missing');
    const reviewed = new Set(reviewInput.review_paths), baseSide = new Set;
    for (const candidate of candidates.values()) {
        if (candidate.old_path) baseSide.add(candidate.old_path);
        if (candidate.change_status === 'deleted') for (const p of candidatePaths(candidate)) baseSide.add(p);
    }
    const currentCheck = context.current_file || (p => currentRegularFile(root, p));
    const baseCheck = context.base_file || (p => baseFile(root, report.implementation_base_commit, p));
    const evidencePath = (p, label) => {
        try { safePath(p, label); } catch (error) { fail(error.message); }
        if (!reviewed.has(p)) fail(label + ' is not in the frozen review input: ' + p);
        if (!currentCheck(p) && (!baseSide.has(p) || !baseCheck(p))) fail(label + ' is neither a safe current file nor a reviewed base blob: ' + p);
    };
    for (const surface of plan.block.surfaces) {
        for (const p of surface.producer_paths) {
            if (surface.change_kind === 'added' ? !currentCheck(p) : surface.change_kind === 'removed' ? !baseCheck(p) : !currentCheck(p) && !baseCheck(p)) fail('planned producer is neither a safe current file nor a valid base blob: ' + p);
        }
        for (const p of [...surface.consumer_paths, ...(surface.compatibility?.old_consumer_paths || []), ...(surface.compatibility?.replacement_consumer_paths || [])]) {
            if (!currentCheck(p)) fail('planned consumer is not a safe current regular file: ' + p);
        }
    }
    const blocking = new Map;
    for (const item of evaluation.blocking_untested) { if (!item || typeof item.id !== 'string' || !item.id || blocking.has(item.id)) fail('blocking item identity invalid'); blocking.set(item.id, item); }
    const findings = new Map;
    for (const finding of evaluation.change_review?.findings || []) { if (!finding || typeof finding.id !== 'string' || findings.has(finding.id)) fail('finding identity invalid'); findings.set(finding.id, finding); }
    const usedCommands = new Set;
    const commandIds = (values, label) => { ids(values, label); for (const id of values) { if (!commands.has(id)) fail(label + ' references unknown command: ' + id); usedCommands.add(id); } return values; };

    const inventory = integration.inventory_assessment;
    object(inventory, ['result', 'reason', 'evidence_paths', 'evidence_command_ids'], 'inventory assessment');
    if (inventory.result !== 'Pass') fail('inventory assessment must Pass before finish'); nonempty(inventory.reason, 'inventory reason');
    exactPaths(inventory.evidence_paths, report.changed_production_paths, 'inventory evidence paths'); inventory.evidence_paths.forEach(p => evidencePath(p, 'inventory evidence path'));
    commandIds(inventory.evidence_command_ids, 'inventory commands'); if (inventory.evidence_command_ids.some(id => commands.get(id).result !== 'Pass')) fail('inventory Pass cites a failed command');

    if (!Array.isArray(integration.orphan_surfaces)) fail('orphan surfaces must be an array');
    set(integration.orphan_surfaces, 'orphan surfaces', assessmentOrder);
    const orphanFields = ['id', 'type', 'reason_code', 'mismatch_kind', 'candidate_ids', 'surface_id', 'kind', 'name', 'producer_paths', 'consumer_paths', 'reason', 'finding_id', 'blocking_untested_ids', 'route'];
    const orphans = new Map, orphanContribution = [];
    for (const orphan of integration.orphan_surfaces) {
        object(orphan, orphanFields, 'orphan surface');
        if (typeof orphan.id !== 'string' || !/^orphan-[a-z0-9]+(?:-[a-z0-9]+)*$/.test(orphan.id) || orphans.has(orphan.id) || !KINDS.includes(orphan.kind)) fail('orphan identity invalid');
        nonempty(orphan.name, 'orphan name'); nonempty(orphan.reason, 'orphan reason'); paths(orphan.producer_paths, 'orphan producer paths'); paths(orphan.consumer_paths, 'orphan consumer paths');
        ids(orphan.candidate_ids, 'orphan candidate IDs'); ids(orphan.blocking_untested_ids, 'orphan blocking IDs');
        if (!orphan.candidate_ids.length && orphan.surface_id === null) fail('orphan has no candidate or surface identity');
        for (const id of orphan.candidate_ids) if (!candidates.has(id)) fail('orphan candidate is unknown');
        let expectedRoute, contribution;
        if (orphan.type === 'candidate' && ['unplanned_surface', 'unmapped_requirement'].includes(orphan.reason_code)) {
            if (!orphan.candidate_ids.length || orphan.surface_id !== null || orphan.mismatch_kind !== null || orphan.blocking_untested_ids.length) fail('candidate orphan fields invalid');
            const reviewableCandidate = id =>
                unmatched.has(id) ||
                report.discovery_mode === 'reviewed_inventory' &&
                typed.has(id) &&
                ['path', 'structural'].includes(candidates.get(id).source);
            if (orphan.candidate_ids.some(id => !reviewableCandidate(id))) fail('candidate orphan must reference an unmatched candidate or a reviewed coarse mapped candidate');
            const expectedPaths=[...new Set(orphan.candidate_ids.flatMap(id=>candidatePaths(candidates.get(id))))].sort(cmp);exactPaths(orphan.producer_paths,expectedPaths,'candidate orphan producer paths'); expectedRoute = 'Planner'; contribution = 'Blocked';
        } else if (orphan.type === 'planned_surface' && ['missing_consumer', 'unrelated_evidence', 'missing_evidence_role'].includes(orphan.reason_code)) {
            if (!plan.surfaces.has(orphan.surface_id) || orphan.mismatch_kind !== null || orphan.blocking_untested_ids.length) fail('planned surface failure orphan invalid'); expectedRoute = 'Generator'; contribution = 'Fail';
        } else if (orphan.type === 'planned_surface' && orphan.reason_code === 'planning_mismatch') {
            if (!plan.surfaces.has(orphan.surface_id) || !['wrong_requirement', 'wrong_task', 'wrong_classification', 'wrong_scope'].includes(orphan.mismatch_kind) || orphan.blocking_untested_ids.length) fail('planning mismatch orphan invalid'); expectedRoute = 'Planner'; contribution = 'Blocked';
        } else if (orphan.type === 'planned_surface' && orphan.reason_code === 'blocked_environment') {
            if (!plan.surfaces.has(orphan.surface_id) || orphan.mismatch_kind !== null || orphan.finding_id !== null || !orphan.blocking_untested_ids.length || orphan.blocking_untested_ids.some(id => !blocking.has(id))) fail('environment orphan invalid'); expectedRoute = 'Environment'; contribution = 'Blocked';
        } else fail('orphan discriminator invalid');
        if (orphan.route !== expectedRoute) fail('orphan route invalid');
        if (orphan.surface_id !== null) {
            const surface = plan.surfaces.get(orphan.surface_id);
            if (orphan.kind !== surface.kind || orphan.name !== surface.name || canonical(orphan.producer_paths) !== canonical(surface.producer_paths) || canonical(orphan.consumer_paths) !== canonical(surface.consumer_paths)) fail('planned orphan surface details drifted');
        }
        if (orphan.finding_id !== null) {
            const finding = findings.get(orphan.finding_id);
            if (!finding || finding.status !== 'Open' || !['Critical', 'Important'].includes(finding.severity) || finding.return_to !== expectedRoute) fail('orphan finding is not an open blocking review finding');
            if (expectedRoute === 'Planner' && (finding.stage !== 'specification_compliance' || finding.category !== 'specification')) fail('Planner orphan needs a specification finding');
        } else if (expectedRoute !== 'Environment') fail('non-environment orphan needs a finding');
        orphan.producer_paths.forEach(p => evidencePath(p, 'orphan producer path')); orphan.consumer_paths.filter(p => reviewed.has(p)).forEach(p => evidencePath(p, 'orphan consumer path'));
        orphanContribution.push(contribution); orphans.set(orphan.id, orphan);
    }

    if (!Array.isArray(integration.candidate_assessments)) fail('candidate assessments must be an array');
    set(integration.candidate_assessments, 'candidate assessments', assessmentOrder);
    const candidateAssessmentFields = ['candidate_id', 'source', 'disposition', 'surface_ids', 'surface_bindings', 'reason', 'producer_paths', 'implementation_consumer', 'evidence_paths', 'evidence_command_ids', 'orphan_ids'];
    const assessedCandidates = new Set, orphanReferences = new Map;
    const referenceOrphan = (id, owner) => { if (!orphans.has(id)) fail('assessment references unknown orphan: ' + id); const list = orphanReferences.get(id) || []; list.push(owner); orphanReferences.set(id, list); };
    for (const assessment of integration.candidate_assessments) {
        object(assessment, candidateAssessmentFields, 'candidate assessment');
        const candidate = candidates.get(assessment.candidate_id); if (!candidate || assessedCandidates.has(assessment.candidate_id) || assessment.source !== candidate.source) fail('candidate assessment identity invalid'); assessedCandidates.add(assessment.candidate_id);
        nonempty(assessment.reason, 'candidate assessment reason'); paths(assessment.producer_paths, 'candidate producer paths'); paths(assessment.evidence_paths, 'candidate evidence paths'); commandIds(assessment.evidence_command_ids, 'candidate evidence commands'); ids(assessment.surface_ids, 'candidate surface IDs'); ids(assessment.orphan_ids, 'candidate orphan IDs');
        if (assessment.evidence_command_ids.some(id => commands.get(id).result !== 'Pass')) fail('candidate assessment cannot consume a failed command');
        assessment.evidence_paths.forEach(p => evidencePath(p, 'candidate evidence path')); assessment.orphan_ids.forEach(id => referenceOrphan(id, 'candidate:' + assessment.candidate_id));
        const logicalCandidatePaths = candidatePaths(candidate);
        if (logicalCandidatePaths.some(p => !assessment.evidence_paths.includes(p))) fail('candidate evidence omits a logical diff path');
        if (!Array.isArray(assessment.surface_bindings)) fail('candidate surface bindings must be an array'); set(assessment.surface_bindings, 'candidate assessment bindings', (a, b) => cmp(a.surface_id, b.surface_id));
        const expectedTyped = typed.get(assessment.candidate_id) || [], expectedSurfaceIds = [...new Set(expectedTyped.map(x => x.surface_id))].sort(cmp);
        if (assessment.disposition === 'mapped') {
            if (!expectedTyped.length || assessment.implementation_consumer !== null) fail('mapped candidate disposition invalid');
            for (const orphanId of assessment.orphan_ids) {
                const orphan = orphans.get(orphanId);
                if (report.discovery_mode !== 'reviewed_inventory' || !['path', 'structural'].includes(candidate.source) || orphan?.type !== 'candidate' || !orphan.candidate_ids.includes(candidate.candidate_id)) fail('mapped coarse candidate orphan backlink invalid');
            }
            sameSet(assessment.surface_ids, expectedSurfaceIds, 'mapped candidate surface IDs'); sameSet(assessment.surface_bindings.map(x => x.surface_id), expectedSurfaceIds, 'mapped candidate bindings');
            for (const binding of assessment.surface_bindings) {
                object(binding, ['surface_id', 'candidate_roles', 'consumer_kind', 'consumer_paths'], 'candidate assessment binding');
                const surface = plan.surfaces.get(binding.surface_id), expectedRoles = [...new Set(expectedTyped.filter(x => x.surface_id === binding.surface_id).map(x => x.role))].sort((a, b) => ['producer', 'consumer'].indexOf(a) - ['producer', 'consumer'].indexOf(b));
                sameSet(binding.candidate_roles, expectedRoles, 'candidate roles', (a, b) => ['producer', 'consumer'].indexOf(a) - ['producer', 'consumer'].indexOf(b));
                if (binding.consumer_kind !== surface.consumer_kind) fail('candidate consumer kind drifted'); exactPaths(binding.consumer_paths, surface.consumer_paths, 'candidate consumer paths');
            }
            const expectedProducer = candidatePaths(candidate).filter(p => expectedTyped.some(x => x.role === 'producer' && plan.surfaces.get(x.surface_id).producer_paths.includes(p)));
            exactPaths(assessment.producer_paths,expectedProducer,'mapped candidate producer paths');
            if (!expectedTyped.some(x => x.role === 'producer')) {
                if (assessment.producer_paths.length || !logicalCandidatePaths.every(p => expectedTyped.some(x => x.role === 'consumer' && plan.surfaces.get(x.surface_id).consumer_paths.includes(p)))) fail('pure consumer candidate path/binding mismatch');
            }
        } else {
            if (expectedTyped.length || assessment.surface_ids.length || assessment.surface_bindings.length) fail('unmatched disposition used for a bound candidate');
            if (assessment.disposition === 'implementation_detail') {
                if (!canBePrivateImplementation(candidate)) fail('surface-bearing candidate cannot be hidden as an implementation detail');
                object(assessment.implementation_consumer, ['consumer_kind', 'consumer_paths'], 'implementation consumer');
                if (assessment.implementation_consumer.consumer_kind !== 'production_caller' || !assessment.implementation_consumer.consumer_paths.length) fail('implementation detail needs a production caller'); paths(assessment.implementation_consumer.consumer_paths, 'implementation consumer paths');
                if (assessment.implementation_consumer.consumer_paths.some(p => !currentCheck(p)||!isProduction(p))) fail('implementation consumer is not a safe current production file');for(const p of assessment.implementation_consumer.consumer_paths){evidencePath(p,'implementation consumer path');if(!assessment.evidence_paths.includes(p))fail('implementation consumer is absent from candidate evidence paths')}
                if (!assessment.producer_paths.length || assessment.orphan_ids.length || !assessment.evidence_command_ids.some(id => ['build', 'test', 'behavior'].includes(commands.get(id).kind))) fail('implementation detail needs independent executable evidence');
            } else if (assessment.disposition === 'private_removal') {
                if (!canBePrivateImplementation(candidate)) fail('surface-bearing candidate cannot be hidden as a private removal');
                if (candidate.change_status !== 'deleted' || assessment.implementation_consumer !== null || !assessment.producer_paths.length || assessment.orphan_ids.length || !assessment.evidence_command_ids.some(id => ['build', 'test', 'behavior'].includes(commands.get(id).kind))) fail('private removal needs independent executable evidence');
            } else if (assessment.disposition === 'non_semantic_change') {
                if (!['path', 'structural'].includes(candidate.source) || assessment.implementation_consumer !== null || !assessment.producer_paths.length || assessment.orphan_ids.length) fail('non-semantic disposition invalid');
            } else if (assessment.disposition === 'orphan') {
                if (assessment.implementation_consumer !== null || !assessment.producer_paths.length || assessment.orphan_ids.length !== 1 || candidate.source === 'clang_ast' && candidate.candidate_scope !== 'reviewable') fail('candidate orphan disposition invalid');
                const orphan = orphans.get(assessment.orphan_ids[0]); if (orphan?.type !== 'candidate' || !orphan.candidate_ids.includes(candidate.candidate_id)) fail('candidate orphan backlink invalid');
            } else fail('candidate disposition invalid');
            exactPaths(assessment.producer_paths,logicalCandidatePaths,'unmatched candidate producer paths');
        }
        if (!assessment.evidence_paths.length) fail('candidate assessment needs reviewed path evidence');
    }
    sameSet([...assessedCandidates].sort(cmp), [...candidates.keys()].sort(cmp), 'candidate assessments');
    for (const [id, owners] of orphanReferences) {
        const orphan = orphans.get(id);
        if (orphan?.type === 'candidate' && (owners.length !== 1 || orphan.candidate_ids.length !== 1 || owners[0] !== 'candidate:' + orphan.candidate_ids[0])) fail('candidate orphan must have one exact candidate assessment owner: ' + id);
    }

    if (!Array.isArray(integration.surface_assessments)) fail('surface assessments must be an array');
    set(integration.surface_assessments, 'surface assessments', assessmentOrder);
    const surfaceAssessmentFields = ['surface_id', 'result', 'reason', 'consumer_paths', 'old_consumer_paths', 'replacement_consumer_paths', 'kind_evidence', 'role_evidence', 'evidence_command_ids', 'blocking_untested_ids', 'orphan_ids'];
    const assessedSurfaces = new Set, surfaceResults = [], surfaceCommandCoverage = new Map;
    for (const assessment of integration.surface_assessments) {
        object(assessment, surfaceAssessmentFields, 'surface assessment');
        const surface = plan.surfaces.get(assessment.surface_id); if (!surface || assessedSurfaces.has(assessment.surface_id)) fail('surface assessment identity invalid'); assessedSurfaces.add(assessment.surface_id);
        result(assessment.result, 'surface assessment'); nonempty(assessment.reason, 'surface assessment reason');
        exactPaths(assessment.consumer_paths, surface.consumer_paths, 'surface consumer paths'); exactPaths(assessment.old_consumer_paths, surface.compatibility?.old_consumer_paths || [], 'surface old consumer paths'); exactPaths(assessment.replacement_consumer_paths, surface.compatibility?.replacement_consumer_paths || [], 'surface replacement consumer paths');
        if (!Array.isArray(assessment.kind_evidence) || !Array.isArray(assessment.role_evidence)) fail('surface evidence matrices missing');
        set(assessment.kind_evidence, 'surface kind evidence', (a, b) => VERIFY_KINDS.indexOf(a.kind) - VERIFY_KINDS.indexOf(b.kind)); set(assessment.role_evidence, 'surface role evidence', (a, b) => ROLE_ORDER.indexOf(a.role) - ROLE_ORDER.indexOf(b.role));
        const kindIds = [], roleIds = [];
        for (const item of assessment.kind_evidence) {
            object(item, ['kind', 'evidence_command_ids'], 'surface kind evidence'); if (!surface.verify_kinds.includes(item.kind)) fail('surface kind evidence is not planned');
            commandIds(item.evidence_command_ids, 'surface kind commands'); if (!item.evidence_command_ids.length || item.evidence_command_ids.some(id => commands.get(id).kind !== item.kind || !commands.get(id).surface_ids.includes(surface.id))) fail('surface kind command binding mismatch'); kindIds.push(...item.evidence_command_ids);
        }
        for (const item of assessment.role_evidence) {
            object(item, ['role', 'evidence_command_ids'], 'surface role evidence'); if (!requiredRoles(surface).includes(item.role)) fail('surface role evidence is not planned');
            commandIds(item.evidence_command_ids, 'surface role commands'); if (!item.evidence_command_ids.length || item.evidence_command_ids.some(id => !commands.get(id).surface_evidence_roles.some(pair => pair.surface_id === surface.id && pair.role === item.role))) fail('surface role command binding mismatch'); roleIds.push(...item.evidence_command_ids);
        }
        ids(assessment.evidence_command_ids, 'surface command IDs'); sameSet(assessment.evidence_command_ids, [...new Set([...kindIds, ...roleIds])].sort(cmp), 'surface command union');
        surfaceCommandCoverage.set(surface.id, new Set(assessment.evidence_command_ids));
        ids(assessment.blocking_untested_ids, 'surface blocking IDs'); if (assessment.blocking_untested_ids.some(id => !blocking.has(id))) fail('surface references unknown blocking item');
        ids(assessment.orphan_ids, 'surface orphan IDs'); assessment.orphan_ids.forEach(id => referenceOrphan(id, 'surface:' + assessment.surface_id));
        const linkedOrphans = assessment.orphan_ids.map(id => orphans.get(id)); if (linkedOrphans.some(x => x?.surface_id !== surface.id)) fail('surface orphan backlink invalid');
        const cited = assessment.evidence_command_ids.map(id => commands.get(id));
        if (assessment.result === 'Pass') {
            sameSet(assessment.kind_evidence.map(x => x.kind), surface.verify_kinds, 'surface verify kinds', (a, b) => VERIFY_KINDS.indexOf(a) - VERIFY_KINDS.indexOf(b));
            sameSet(assessment.role_evidence.map(x => x.role), requiredRoles(surface), 'surface evidence roles', (a, b) => ROLE_ORDER.indexOf(a) - ROLE_ORDER.indexOf(b));
            const plannedPairs = surface.verify_kinds.flatMap(kind => requiredRoles(surface).map(role => kind + '\0' + role)).sort(cmp);
            const coveredPairs = [...new Set(cited.flatMap(command =>
                command.surface_probe_bindings
                    .filter(binding => binding.surface_id === surface.id)
                    .map(binding => command.kind + '\0' + binding.role)
            ))].sort(cmp);
            sameSet(coveredPairs, plannedPairs, 'surface probe kind/role matrix');
            if (assessment.blocking_untested_ids.length || assessment.orphan_ids.length || cited.some(x => x.result !== 'Pass')) fail('Pass surface carries failure or blocking evidence');
        } else if (assessment.result === 'Fail') {
            if (assessment.blocking_untested_ids.length || !cited.some(x => x.result === 'Fail') && !linkedOrphans.some(x => ['missing_consumer', 'unrelated_evidence', 'missing_evidence_role'].includes(x?.reason_code))) fail('Fail surface lacks failure evidence');
        } else {
            if (cited.some(x => x.result === 'Fail')) fail('Blocked surface cannot hide a failed command; use Fail');
            if (!assessment.blocking_untested_ids.length && !linkedOrphans.some(x => ['planning_mismatch', 'blocked_environment', 'unplanned_surface', 'unmapped_requirement'].includes(x?.reason_code))) fail('Blocked surface lacks a blocker');
        }
        surfaceResults.push(assessment.result);
    }
    sameSet([...assessedSurfaces].sort(cmp), plan.block.surfaces.map(surface => surface.id), 'surface assessments');
    for (const [id, owners] of orphanReferences) if (owners.length !== 1) fail('orphan must be referenced by exactly one assessment: ' + id);
    sameSet([...orphanReferences.keys()].sort(cmp), [...orphans.keys()].sort(cmp), 'orphan assessment backlinks');
    for(const provisional of directClosure?.provisionally_blocked||[]){const assessment=integration.surface_assessments.find(item=>item.surface_id===provisional.surface_id),blockingItem=blocking.get(provisional.exception_id),linked=(assessment?.orphan_ids||[]).map(id=>orphans.get(id));if(!assessment||assessment.result!=='Blocked'||!blockingItem||!Array.isArray(blockingItem.task_ids)||!blockingItem.task_ids.includes(provisional.task_id)||!assessment.blocking_untested_ids.includes(provisional.exception_id)||!linked.some(orphan=>orphan?.reason_code==='blocked_environment'&&orphan.blocking_untested_ids.includes(provisional.exception_id)))fail(`provisional Generator obligation was not preserved as an Environment blocker: ${provisional.surface_id}/${provisional.task_id}/${provisional.kind}/${provisional.role}`)}

    for (const command of commands.values()) for (const surfaceId of command.surface_ids) {
        if (!surfaceCommandCoverage.get(surfaceId)?.has(command.id)) fail('surface-bound Evaluation command is not consumed by its bound surface: ' + command.id + '/' + surfaceId);
    }
    const surfaceBoundCommands = [...commands.values()].filter(command => command.surface_ids.length).map(command => command.id).sort(cmp);
    if (surfaceBoundCommands.some(id => !usedCommands.has(id))) fail('surface-bound Evaluation command is not consumed: ' + surfaceBoundCommands.find(id => !usedCommands.has(id)));
    const integrationResult = surfaceResults.includes('Fail') || orphanContribution.includes('Fail') ? 'Fail' : surfaceResults.includes('Blocked') || orphanContribution.includes('Blocked') ? 'Blocked' : 'Pass';
    if (integration.result !== integrationResult) fail('Integration result does not follow Fail-first aggregation');
    const componentResults = [integrationResult, evaluation.implementation_economy?.result, ...(evaluation.criteria || []).map(item => item.status), ...(evaluation.change_review?.stages || []).map(stage => stage.status === 'NotRun' ? 'Blocked' : stage.status)].filter(value => ['Pass', 'Fail', 'Blocked'].includes(value));
    const expectedVerdict = componentResults.includes('Fail') ? 'Fail' : componentResults.includes('Blocked') ? 'Blocked' : 'Pass';
    if (evaluation.verdict !== expectedVerdict) fail('top-level verdict does not follow unified Fail-first aggregation');
    const plannerFindingIds = integration.orphan_surfaces.filter(orphan => orphan.route === 'Planner' && orphan.finding_id !== null).map(orphan => orphan.finding_id).sort(cmp);
    if (plannerFindingIds.length) {
        const specification = evaluation.change_review?.stages?.find(stage => stage.name === 'specification_compliance');
        if (!specification || specification.status !== 'Blocked') fail('Planner-routed Integration findings must narrow specification review to Blocked');
    }
    return { result: integrationResult, planning_block_sha256: plan.block_sha256, report_sha256: reportSha, discovery_identity_sha256: discoverySha, used_command_ids: [...usedCommands].sort(cmp), surface_bound_command_ids: surfaceBoundCommands, planner_blocking_finding_ids: plannerFindingIds };
}

/*
 * Validate the complete v3 evidence family without writing to the change.
 * Active finish/recheck and post-archive recovery deliberately share this
 * function so review, criteria, economy, inventory and freshness rules cannot
 * drift into two different contracts.
 */
function validateCompleteEvaluationV3(context) {
    const root = fs.realpathSync(path.resolve(context.root || process.cwd()));
    const change = context.change;
    const changeRoot = path.resolve(context.change_root || path.join(root, 'openspec', 'changes', change));
    const fail = message => { throw Error('Complete Evaluation v3: ' + message); };
    if (!CHANGE_ID.test(change || '')) fail('invalid change ID');
    const plan = parsePlanFromChangeRoot(root, change, changeRoot);
    const manifest = require(path.join(root, 'scripts', 'manifest_policy.js'));
    const planning = manifest.planningStateAt(root, change, changeRoot);
    const harness = path.join(changeRoot, 'harness');
    const regular = (file, label) => {
        const st = fs.lstatSync(file);
        if (!st.isFile() || st.isSymbolicLink()) fail('unsafe ' + label);
        return file;
    };
    const readCanonical = (file, label) => {
        regular(file, label);
        const raw = fs.readFileSync(file), value = JSON.parse(raw);
        if (!raw.equals(Buffer.from(JSON.stringify(value, null, 2) + '\n'))) fail(label + ' bytes are noncanonical');
        return { raw, value };
    };
    const baselineRecord = readCanonical(path.join(harness, 'evaluation-baseline.json'), 'Evaluation baseline');
    const evaluationRecord = readCanonical(path.join(harness, 'evaluation.json'), 'Evaluation');
    const ledgerRecord = readCanonical(path.join(harness, 'evaluation-command-ledger.json'), 'Evaluation ledger');
    const reportRecord = readCanonical(path.join(harness, 'integration-surface-report.json'), 'Integration report');
    const snapshot = readCanonical(path.join(harness, 'ai_snapshot.json'), 'change snapshot').value;
    const verificationRecord = readCanonical(path.join(harness, 'verification.json'), 'verification');
    const footprintRecord = readCanonical(path.join(harness, 'change-footprint.json'), 'change footprint');
    const baseline = baselineRecord.value, evaluation = evaluationRecord.value, ledger = ledgerRecord.value;
    const report = reportRecord.value, verification = verificationRecord.value, footprint = footprintRecord.value;
    const same = (a, b, label) => { if (canonical(a) !== canonical(b)) fail(label + ' mismatch'); };
    for (const [supplied, actual, label] of [
        [context.baseline, baseline, 'supplied baseline'], [context.evaluation, evaluation, 'supplied Evaluation'],
        [context.ledger, ledger, 'supplied ledger'], [context.report, report, 'supplied report']
    ]) if (supplied !== undefined) same(supplied, actual, label);
    if (context.report_raw !== undefined && !Buffer.from(context.report_raw).equals(reportRecord.raw)) fail('supplied report bytes mismatch');

    const own = (object, key) => Object.prototype.hasOwnProperty.call(object, key);
    const closedObject = (object, fields, label, required = fields) => {
        if (!object || typeof object !== 'object' || Array.isArray(object) ||
            Object.keys(object).some(key => !fields.includes(key)) || required.some(key => !own(object, key))) fail(label + ' closed schema');
    };
    const digest = (value, label) => { if (!DIGEST.test(value || '')) fail(label + ' digest invalid'); return value; };
    const validTime = value => Number.isFinite(Date.parse(value)) && Date.parse(value) <= Date.now() + 300000;
    const result = (value, label) => { if (!['Pass', 'Fail', 'Blocked'].includes(value)) fail(label + ' result invalid'); return value; };
    const exact = (actual, expected, label) => {
        if (!Array.isArray(actual) || !Array.isArray(expected) ||
            actual.length !== new Set(actual.map(canonical)).size || expected.length !== new Set(expected.map(canonical)).size ||
            canonical([...actual].sort((a, b) => cmp(canonical(a), canonical(b)))) !== canonical([...expected].sort((a, b) => cmp(canonical(a), canonical(b))))) {
            fail(label + ' coverage mismatch');
        }
    };
    const safeEvidencePath = (value, label, allowPlanning = false) => {
        safePath(value, label);
        if (allowPlanning && value.startsWith('openspec/specs/') && value.endsWith('/spec.md')) {
            regular(path.join(root, ...value.split('/')), label);
            return;
        }
        if (allowPlanning && value.startsWith(`openspec/changes/${change}/`)) {
            const relative = value.slice(`openspec/changes/${change}/`.length);
            const allowed = relative === '.openspec.yaml' || ['proposal.md', 'design.md', 'tasks.md'].includes(relative) ||
                relative.startsWith('specs/') && relative.endsWith('.md');
            if (!allowed) fail(label + ' is not a canonical planning artifact');
            regular(path.join(changeRoot, ...relative.split('/')), label);
            return;
        }
        const reviewed = new Set(evaluation.review_input?.review_paths || []);
        if (!reviewed.has(value)) fail(label + ' is not in the frozen review input');
        const current = path.join(root, ...value.split('/'));
        try {
            const st = fs.lstatSync(current);
            if (!st.isFile() || st.isSymbolicLink()) fail(label + ' is not a current regular file');
        } catch (error) {
            if (error.message?.startsWith('Complete Evaluation v3:')) throw error;
            try { cp.execFileSync('git', ['cat-file', '-e', `${snapshot.implementation_base_commit}:${value}`], { cwd: root, stdio: 'ignore' }); }
            catch { fail(label + ' is neither a current file nor an implementation-base blob'); }
        }
    };
    const safeRepositoryPath = (value, label) => safePath(value, label);
    const secret = /(?:authorization|proxy-authorization|bearer|x-?api-?key|api[_-]?key|token|password|secret|cookie|client[_-]?secret|private[_-]?key|access[_-]?key)[\s:=]+\S+|:\/\/[^/\s:]+:[^/@\s]+@|-----BEGIN [A-Z ]*PRIVATE KEY-----/i;
    const credentialOptions = new Set(['--token', '--password', '--secret', '--api-key', '--apikey', '-H', '--header', '--cookie']);
    const rejectSecrets = value => {
        if (typeof value === 'string') { if (secret.test(value)) fail('secret-like evidence'); return; }
        if (Array.isArray(value)) { value.forEach(rejectSecrets); return; }
        if (value && typeof value === 'object') Object.values(value).forEach(rejectSecrets);
    };

    if (snapshot.schema_version !== 4 || verification.schema_version !== 3 ||
        snapshot.planned_change_fingerprint !== planning.planning_fingerprint ||
        snapshot.planned_tdd_policy_sha256 !== planning.tdd_policy_sha256 ||
        snapshot.planned_integration_completeness_sha256 !== planning.integration_completeness_sha256 ||
        snapshot.planned_base_specs_fingerprint !== baseline.base_specs_fingerprint ||
        !validTime(snapshot.planning_approved_at) ||
        typeof snapshot.implementation_base_commit !== 'string' || !/^[0-9a-f]{40,64}$/.test(snapshot.implementation_base_commit)) {
        fail('approved planning snapshot is missing or stale');
    }
    try {
        if (cp.execFileSync('git', ['rev-parse', '--verify', snapshot.implementation_base_commit + '^{commit}'], { cwd: root, encoding: 'utf8' }).trim() !== snapshot.implementation_base_commit) fail('implementation base is not exact');
        cp.execFileSync('git', ['merge-base', '--is-ancestor', snapshot.implementation_base_commit, 'HEAD'], { cwd: root, stdio: 'ignore' });
    } catch (error) {
        if (error.message?.startsWith('Complete Evaluation v3:')) throw error;
        fail('implementation base is invalid');
    }

    const sourceFingerprint = cp.execFileSync(path.join(root, 'scripts', 'source_fingerprint.sh'), ['--kind', 'source'], { cwd: root, encoding: 'utf8' }).trim();
    digest(sourceFingerprint, 'current source fingerprint');
    const artifactFingerprintAt = () => {
        const records = [];
        const visit = (absolute, logical) => {
            const st = fs.lstatSync(absolute);
            if (st.isSymbolicLink()) fail('planning artifact symlink');
            if (st.isDirectory()) {
                for (const name of fs.readdirSync(absolute).sort(cmp)) visit(path.join(absolute, name), logical + '/' + name);
            } else if (st.isFile() && (['.openspec.yaml', 'proposal.md', 'design.md', 'tasks.md'].includes(logical) || logical.startsWith('specs/') && logical.endsWith('.md'))) {
                const repositoryPath = `openspec/changes/${change}/${logical}`;
                const mode = (st.mode & 0o111) ? '100755' : '100644';
                records.push(`${repositoryPath}\0${mode}\0file\0${sha(fs.readFileSync(absolute))}\0`);
            }
        };
        for (const name of ['.openspec.yaml', 'proposal.md', 'design.md', 'tasks.md', 'specs']) visit(path.join(changeRoot, name), name);
        records.sort(cmp);
        return sha(Buffer.from(records.length ? records.join('') : '<empty>\0'));
    };
    const artifactFingerprint = artifactFingerprintAt();
    const baselineCore = ['schema_version', 'evaluation_id', 'change_name', 'status', 'started_at', 'source_fingerprint', 'artifact_fingerprint', 'base_specs_fingerprint', 'verification_json_sha256', 'budget_block_sha256', 'change_footprint_json_sha256', 'integration_planning_block_sha256', 'integration_surface_report_sha256', 'integration_discovery_identity_sha256', 'review_input'];
    const baselineTail = baseline.status === 'complete' ? ['completed_at', 'evaluation_json_sha256'] : baseline.status === 'in_progress' ? [] : null;
    if (!baselineTail) fail('baseline must be in-progress or complete');
    closedObject(baseline, [...baselineCore, ...baselineTail], 'Evaluation baseline');
    if (baseline.schema_version !== 3 || baseline.change_name !== change || !/^eval-\d{8}T\d{6}Z-[0-9a-f]{6}$/.test(baseline.evaluation_id) ||
        !validTime(baseline.started_at) || baseline.source_fingerprint !== sourceFingerprint ||
        baseline.artifact_fingerprint !== artifactFingerprint ||
        baseline.verification_json_sha256 !== sha(verificationRecord.raw) ||
        baseline.change_footprint_json_sha256 !== sha(footprintRecord.raw) ||
        baseline.integration_planning_block_sha256 !== plan.block_sha256 ||
        baseline.integration_surface_report_sha256 !== sha(reportRecord.raw) ||
        baseline.integration_discovery_identity_sha256 !== discoveryIdentity(report)) fail('baseline identity, digest or freshness drift');
    if (baseline.status === 'complete' && (!validTime(baseline.completed_at) ||
        Date.parse(baseline.completed_at) < Date.parse(baseline.started_at) ||
        baseline.evaluation_json_sha256 !== sha(evaluationRecord.raw))) fail('completed baseline terminal identity drift');
    for (const key of ['source_fingerprint', 'artifact_fingerprint', 'base_specs_fingerprint']) {
        if (evaluation['input_' + key] !== baseline[key] || evaluation[key] !== baseline[key]) fail('Evaluation ' + key + ' drift');
    }
    if (evaluation.schema_version !== 3 || evaluation.evaluation_id !== baseline.evaluation_id ||
        evaluation.change_name !== change || evaluation.evaluation_started_at !== baseline.started_at ||
        evaluation.openspec_version !== '1.6.0' || evaluation.evaluator_role !== 'independent' ||
        !validTime(evaluation.evaluated_at) || Date.parse(evaluation.evaluated_at) < Date.parse(baseline.started_at) ||
        baseline.status === 'complete' && Date.parse(evaluation.evaluated_at) > Date.parse(baseline.completed_at) ||
        evaluation.budget_block_sha256 !== baseline.budget_block_sha256 ||
        evaluation.change_footprint_json_sha256 !== baseline.change_footprint_json_sha256) fail('Evaluation identity or digest drift');
    rejectSecrets(evaluation);

    const currentReview = manifest.reviewInput(root, change, snapshot.implementation_base_commit, {
        source_fingerprint: baseline.source_fingerprint,
        artifact_fingerprint: baseline.artifact_fingerprint,
        base_specs_fingerprint: baseline.base_specs_fingerprint
    });
    same(baseline.review_input, currentReview, 'baseline review input freshness');
    same(evaluation.review_input, baseline.review_input, 'Evaluation review input');
    const reviewPaths = evaluation.review_input.review_paths;
    if (!Array.isArray(reviewPaths) || reviewPaths.length !== new Set(reviewPaths).size) fail('review path inventory invalid');

    const tddClosure = manifest.verifyTddEvidenceAt(root, change, changeRoot, { requireDone: true, sourceFingerprint });
    const directClosure = verifyIntegrationEvidenceFromChangeRoot(root, change, changeRoot, { requireDone: true, sourceFingerprint });
    if (directClosure.provisionally_blocked.length && evaluation.verdict === 'Pass') fail('provisional Generator closure cannot Pass');
    const tasks = plan.tasks;
    if ([...tasks.values()].some(task => !task.done)) fail('all tasks must be complete');
    const deltaUniverse = parseDeltaUniverse(changeRoot);
    exact([...new Set([...tasks.values()].flatMap(task => task.refKeys))], [...deltaUniverse], 'task/delta references');
    closedObject(verification, ['schema_version', 'change_name', 'migration', 'tasks'], 'verification');
    if (verification.schema_version !== 3 || verification.change_name !== change || !Array.isArray(verification.tasks)) fail('verification identity');
    const verificationTasks = new Map;
    for (const task of verification.tasks) {
        closedObject(task, ['task_id', 'requirement_refs', 'surface_ids', 'changed_paths', 'footprint_observation', 'commands'], 'verification task');
        if (typeof task.task_id !== 'string' || verificationTasks.has(task.task_id) || !tasks.has(task.task_id) ||
            !Array.isArray(task.requirement_refs) || !Array.isArray(task.surface_ids) ||
            !Array.isArray(task.changed_paths) || !Array.isArray(task.commands) || !task.commands.length) fail('verification task contract');
        exact(task.requirement_refs.map(refKey), tasks.get(task.task_id).refKeys, 'verification task requirement refs');
        task.changed_paths.forEach(value => safePath(value, 'verification changed path'));
        closedObject(task.footprint_observation, ['status', 'drift_reason'], 'verification footprint observation');
        if (!['within_expected', 'drift_warning'].includes(task.footprint_observation.status) ||
            task.footprint_observation.status === 'within_expected' && task.footprint_observation.drift_reason !== null ||
            task.footprint_observation.status === 'drift_warning' &&
            (typeof task.footprint_observation.drift_reason !== 'string' || !task.footprint_observation.drift_reason.trim())) {
            fail('verification footprint observation contract');
        }
        verificationTasks.set(task.task_id, task);
    }
    exact([...verificationTasks.keys()], [...tasks.keys()], 'verification task coverage');

    const economyPlan = parseEconomy(fs.readFileSync(path.join(changeRoot, 'design.md'), 'utf8'));
    const compiledClassification = scopeLib.compileClassification(economyPlan.classification);
    const isProduction = value => compiledClassification.patterns.production.some(pattern => pattern.test(value));
    const scope = scopeLib.collectScope(root, snapshot.implementation_base_commit, economyPlan.classification, economyPlan.schema_version);
    if (scope.unclassified_paths.length || scope.classification_overlaps.length) fail('scope classification is invalid');
    const implementationPaths = [...new Set(scope.logical_changes.flatMap(item => [item.path, ...(item.old_path ? [item.old_path] : [])]))].sort(cmp);
    const declaredImplementationPaths = new Set(verification.tasks.flatMap(task => task.changed_paths));
    const undeclaredImplementationPaths = implementationPaths.filter(value => !declaredImplementationPaths.has(value));
    if (undeclaredImplementationPaths.length) fail('undeclared implementation paths: ' + undeclaredImplementationPaths.join(', '));
    if (footprint.schema_version !== 1 || footprint.change_name !== change ||
        footprint.implementation_base_commit !== snapshot.implementation_base_commit ||
        footprint.source_fingerprint !== sourceFingerprint ||
        footprint.budget_block_sha256 !== sha(Buffer.from(canonical(economyPlan))) ||
        !['within_expected', 'drift_warning'].includes(footprint.status)) fail('footprint identity or freshness drift');
    const expectedFootprintCandidates = scope.structural_candidates.map(item => ({
        candidate_id: item.candidate_id, path: item.path, kind: item.kind, allowance_kind: item.allowance_kind
    }));
    same(footprint.structural_candidates, expectedFootprintCandidates, 'footprint structural candidates');
    same(footprint.unclassified_paths, scope.unclassified_paths, 'footprint unclassified paths');
    same(footprint.classification_overlaps, scope.classification_overlaps, 'footprint classification overlaps');

    const pathCandidates = scope.logical_changes.filter(item => item.classifications.includes('production')).map(item => {
        const id = crypto.createHash('sha256').update('production-path\0' + item.change_status + '\0' + (item.old_path || '') + '\0' + item.path).digest('hex').slice(0, 16);
        return { candidate_id: 'path-candidate-' + id, source: 'path', path: item.path, old_path: item.old_path, change_status: item.change_status };
    }).sort((a, b) => cmp(a.candidate_id, b.candidate_id));
    const structuralCandidates = scope.structural_candidates.map(item => ({
        candidate_id: item.candidate_id, source: 'structural', path: item.path, kind: item.kind,
        allowance_kind: item.allowance_kind, old_path: item.old_path, change_status: item.change_status
    })).sort((a, b) => cmp(a.candidate_id, b.candidate_id));
    same(report.changed_production_paths, scope.changed_production_paths, 'report production path inventory');
    same(report.path_candidates, pathCandidates, 'report path candidates');
    same(report.structural_candidates, structuralCandidates, 'report structural candidates');
    if (report.scope_classifier_identity.path !== 'scripts/change_scope.js' ||
        report.scope_classifier_identity.sha256 !== sha(fs.readFileSync(path.join(root, 'scripts', 'change_scope.js'))) ||
        report.scope_classifier_identity.output_sha256 !== scope.output_sha256) fail('scope classifier identity or output drift');
    let astCandidates = [];
    if (plan.block.discovery.mode === 'clang_ast') {
        const ast = require(path.join(root, 'scripts', 'clang_ast_surface_adapter.js')).discover({
            root, change, implementationBase: snapshot.implementation_base_commit,
            compileCommandsPath: plan.block.discovery.compile_commands_path, plan, scope,
            classification: economyPlan.classification
        });
        astCandidates = ast.candidates;
        same(report.ast_candidates, ast.candidates, 'AST candidate inventory');
        same(report.ast_tool_identity, ast.ast_tool_identity, 'AST tool identity');
        same(report.discovery_adapter_identity, ast.adapter_identity, 'AST adapter identity');
        if (report.compile_commands_sha256 !== ast.compile_commands_sha256) fail('compile_commands identity drift');
    } else {
        const expectedAdapter = { id: 'reviewed-inventory-v1', schema_version: 1, sha256: sha(fs.readFileSync(path.join(root, 'scripts', 'integration_surface_lib.js'))) };
        same(report.discovery_adapter_identity, expectedAdapter, 'reviewed inventory adapter identity');
    }
    const allCandidates = [...pathCandidates, ...structuralCandidates, ...astCandidates];
    const expectedBindings = [], mappedCandidates = new Map;
    for (const surface of plan.block.surfaces) {
        const candidateBindings = [];
        const consumerPaths = [...surface.consumer_paths, ...(surface.compatibility?.old_consumer_paths || []), ...(surface.compatibility?.replacement_consumer_paths || [])];
        for (const candidate of [...pathCandidates, ...structuralCandidates]) {
            const logicalPaths = [candidate.path, ...(candidate.old_path ? [candidate.old_path] : [])];
            const roles = [];
            if (logicalPaths.some(value => surface.producer_paths.includes(value))) roles.push('producer');
            if (logicalPaths.some(value => consumerPaths.includes(value))) roles.push('consumer');
            for (const role of roles) {
                let treeSide = candidate.change_status === 'added' ? 'current' : candidate.change_status === 'deleted' ? 'base' : 'both';
                if (role === 'producer') treeSide = surface.change_kind === 'added' ? 'current' : surface.change_kind === 'removed' ? 'base' : treeSide;
                candidateBindings.push({ candidate_id: candidate.candidate_id, role, tree_side: treeSide });
                const owners = mappedCandidates.get(candidate.candidate_id) || [];
                owners.push({ surface_id: surface.id, role }); mappedCandidates.set(candidate.candidate_id, owners);
            }
        }
        for (const candidate of astCandidates) {
            const symbols = surface.symbol_identities;
            if (!symbols) continue;
            const baseMatch = candidate.base_symbol_identity !== null && symbols.base.some(identity => canonical(identity) === canonical(candidate.base_symbol_identity));
            const currentMatch = candidate.current_symbol_identity !== null && symbols.current.some(identity => canonical(identity) === canonical(candidate.current_symbol_identity));
            const treeSide = surface.change_kind === 'added' ? 'current' : surface.change_kind === 'removed' ? 'base' : 'both';
            const matches = treeSide === 'current' ? currentMatch : treeSide === 'base' ? baseMatch : baseMatch && currentMatch;
            if (matches) {
                candidateBindings.push({ candidate_id: candidate.candidate_id, role: 'producer', tree_side: treeSide });
                const owners = mappedCandidates.get(candidate.candidate_id) || [];
                owners.push({ surface_id: surface.id, role: 'producer' });
                mappedCandidates.set(candidate.candidate_id, owners);
            }
        }
        candidateBindings.sort((a, b) => cmp(a.candidate_id + a.role + a.tree_side, b.candidate_id + b.role + b.tree_side));
        expectedBindings.push({
            surface_id: surface.id, candidate_bindings: candidateBindings,
            producer_paths: surface.producer_paths, consumer_paths: surface.consumer_paths,
            old_consumer_paths: surface.compatibility?.old_consumer_paths || [],
            replacement_consumer_paths: surface.compatibility?.replacement_consumer_paths || []
        });
    }
    expectedBindings.sort((a, b) => cmp(a.surface_id, b.surface_id));
    const expectedUnmatched = allCandidates.filter(candidate => !mappedCandidates.has(candidate.candidate_id)).map(candidate => ({
        candidate_id: candidate.candidate_id, source: candidate.source,
        reason: candidate.source === 'clang_ast'
            ? 'No exact approved symbol identity matched this declaration candidate.'
            : 'The changed path is not mapped to an approved product surface.'
    })).sort((a, b) => cmp(a.candidate_id, b.candidate_id));
    const expectedStatus = expectedUnmatched.some(item => {
        const candidate = astCandidates.find(value => value.candidate_id === item.candidate_id);
        return candidate?.candidate_scope === 'public_contract';
    }) ? 'orphaned' : expectedUnmatched.length ? 'review_required' : 'complete';
    same(report.surface_candidate_bindings, expectedBindings, 'surface candidate bindings');
    same(report.unmatched_candidates, expectedUnmatched, 'unmatched candidate inventory');
    if (report.status !== expectedStatus) fail('report status drift');
    const integration = validateEvaluationV3({
        root, change, plan, baseline, evaluation, ledger, report, report_raw: reportRecord.raw,
        direct_closure: directClosure, is_production: isProduction,
        classification: economyPlan.classification
    });

    const commands = new Map;
    for (const command of ledger.commands) {
        if (commands.has(command.id) || !Array.isArray(command.argv) || command.argv.some(value => typeof value !== 'string') ||
            command.argv.some(value => credentialOptions.has(value) || secret.test(value)) ||
            typeof command.command !== 'string' || !command.command.trim() ||
            typeof command.expected !== 'string' || !command.expected.trim() ||
            typeof command.observed !== 'string' || !command.observed.trim()) fail('Evaluation command content invalid');
        safePath(command.working_directory, 'Evaluation command working directory');
        if (!validTime(command.started_at) || !validTime(command.finished_at) ||
            Date.parse(command.started_at) < Date.parse(baseline.started_at) ||
            Date.parse(command.finished_at) < Date.parse(command.started_at) ||
            Date.parse(command.finished_at) > Date.parse(evaluation.evaluated_at)) fail('Evaluation command identity or time invalid');
        commands.set(command.id, command);
    }
    const blocking = new Map;
    for (const item of evaluation.blocking_untested) {
        closedObject(item, ['id', 'requirement_refs', 'task_ids', 'reason', 'required_evidence'], 'blocking item');
        if (typeof item.id !== 'string' || !item.id || blocking.has(item.id) ||
            !Array.isArray(item.requirement_refs) || !item.requirement_refs.length ||
            item.requirement_refs.some(ref => !deltaUniverse.has(refKey(ref))) ||
            !Array.isArray(item.task_ids) || !item.task_ids.length || item.task_ids.some(id => !tasks.has(id)) ||
            typeof item.reason !== 'string' || !item.reason.trim() ||
            !Array.isArray(item.required_evidence) || !item.required_evidence.length ||
            item.required_evidence.some(value => typeof value !== 'string' || !value.trim())) fail('blocking item contract');
        const itemRefs = item.requirement_refs.map(refKey);
        for (const taskId of item.task_ids) {
            if (!tasks.get(taskId).refKeys.some(key => itemRefs.includes(key))) fail('blocking task does not cover blocking requirement');
        }
        blocking.set(item.id, item);
    }
    for (const taskId of tddClosure.blocking_exception_task_ids || []) {
        if (![...blocking.values()].some(item => item.task_ids.includes(taskId))) fail('unavailable-environment TDD exception is not blocking: ' + taskId);
    }
    const riskIds = new Set;
    for (const item of evaluation.residual_risks) {
        closedObject(item, ['id', 'impact', 'rationale'], 'residual risk');
        if (typeof item.id !== 'string' || !item.id || riskIds.has(item.id) ||
            typeof item.impact !== 'string' || !item.impact.trim() ||
            typeof item.rationale !== 'string' || !item.rationale.trim()) fail('residual risk contract');
        riskIds.add(item.id);
    }

    const usedCommands = new Set(integration.used_command_ids), usedBlocking = new Set;
    const useCommands = (ids, label, requirePass = false) => {
        if (!Array.isArray(ids) || ids.length !== new Set(ids).size || ids.some(id => !commands.has(id))) fail(label + ' command references');
        if (requirePass && ids.some(id => commands.get(id).result !== 'Pass')) fail(label + ' cites failed command');
        ids.forEach(id => usedCommands.add(id));
    };
    const review = evaluation.change_review;
    closedObject(review, ['schema_version', 'git_state_fingerprint', 'stages', 'findings'], 'change review');
    if (review.schema_version !== 1 || review.git_state_fingerprint !== evaluation.review_input.git_state_fingerprint ||
        !Array.isArray(review.stages) || review.stages.length !== 2 || !Array.isArray(review.findings)) fail('change review identity');
    const stageNames = ['specification_compliance', 'code_quality'];
    const dimensions = {
        specification_compliance: ['requirements', 'scenarios', 'scope', 'contracts', 'traceability'],
        code_quality: ['correctness', 'safety', 'regression_risk', 'reuse', 'complexity', 'test_quality', 'repository_impact']
    };
    const findingCategories = {
        specification_compliance: new Set(['specification']),
        code_quality: new Set(['correctness', 'security', 'data_integrity', 'regression_risk', 'reuse', 'complexity', 'test_quality', 'repository_impact', 'maintainability'])
    };
    const findingMap = new Map, findingsByStage = new Map(stageNames.map(name => [name, []]));
    for (const finding of review.findings) {
        closedObject(finding, ['id', 'stage', 'category', 'severity', 'status', 'summary', 'requirement_refs', 'task_ids', 'evidence_paths', 'evidence_command_ids', 'return_to', 'resolution', 'tracking'], 'review finding');
        if (!/^finding-[a-z0-9]+(?:-[a-z0-9]+)*$/.test(finding.id || '') || findingMap.has(finding.id) ||
            !stageNames.includes(finding.stage) || !findingCategories[finding.stage]?.has(finding.category) ||
            !['Critical', 'Important', 'Minor'].includes(finding.severity) ||
            !['Open', 'Resolved', 'Deferred'].includes(finding.status) || !['Generator', 'Planner'].includes(finding.return_to) ||
            typeof finding.summary !== 'string' || !finding.summary.trim() || finding.summary.length > 500 || /[\r\n]/.test(finding.summary) ||
            !Array.isArray(finding.requirement_refs) || finding.requirement_refs.some(ref => !deltaUniverse.has(refKey(ref))) ||
            !Array.isArray(finding.task_ids) || finding.task_ids.some(id => !tasks.has(id)) ||
            !Array.isArray(finding.evidence_paths) || !Array.isArray(finding.evidence_command_ids) ||
            !finding.evidence_paths.length && !finding.evidence_command_ids.length) fail('review finding contract');
        if (finding.stage === 'specification_compliance' && finding.severity === 'Minor') fail('specification finding cannot be Minor');
        finding.evidence_paths.forEach(value => safeEvidencePath(value, 'finding evidence path', true));
        useCommands(finding.evidence_command_ids, 'finding');
        if (finding.status === 'Open') {
            if (finding.severity === 'Minor' || finding.resolution !== null || finding.tracking !== null) fail('open finding contract');
        } else if (finding.status === 'Resolved') {
            closedObject(finding.resolution, ['summary', 'evidence_paths', 'evidence_command_ids'], 'finding resolution');
            if (typeof finding.resolution.summary !== 'string' || !finding.resolution.summary.trim() ||
                /[\r\n]/.test(finding.resolution.summary) ||
                !Array.isArray(finding.resolution.evidence_paths) || !Array.isArray(finding.resolution.evidence_command_ids) ||
                !finding.resolution.evidence_paths.length && !finding.resolution.evidence_command_ids.length ||
                finding.tracking !== null) fail('resolved finding contract');
            finding.resolution.evidence_paths.forEach(value => safeEvidencePath(value, 'finding resolution path'));
            useCommands(finding.resolution.evidence_command_ids, 'finding resolution', true);
        } else {
            if (finding.severity !== 'Minor' || finding.resolution !== null || !finding.tracking ||
                !['technical_debt', 'residual_risk'].includes(finding.tracking.kind) ||
                typeof finding.tracking.id !== 'string' || !finding.tracking.id) fail('deferred finding contract');
            closedObject(finding.tracking, ['kind', 'id'], 'finding tracking');
            if (finding.tracking.kind === 'residual_risk' && !riskIds.has(finding.tracking.id)) fail('deferred finding risk missing');
            if (finding.tracking.kind === 'technical_debt') {
                const escaped = finding.tracking.id.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
                if (!new RegExp(`^\\|\\s*${escaped}\\s*\\|`, 'm').test(fs.readFileSync(path.join(root, 'debt-register.md'), 'utf8'))) fail('deferred finding debt missing');
            }
        }
        findingMap.set(finding.id, finding); findingsByStage.get(finding.stage).push(finding);
    }
    const historyDirectory = path.join(harness, 'evaluations'), priorEvaluations = [], terminalTimes = new Map;
    if (fs.existsSync(historyDirectory)) {
        const historyStat = fs.lstatSync(historyDirectory);
        if (!historyStat.isDirectory() || historyStat.isSymbolicLink()) fail('unsafe Evaluation history directory');
        for (const name of fs.readdirSync(historyDirectory)) {
            if (!/^eval-\d{8}T\d{6}Z-[0-9a-f]{6}\.json$/.test(name)) fail('unexpected Evaluation history entry');
            const record = readCanonical(path.join(historyDirectory, name), 'Evaluation history entry').value;
            closedObject(record, ['envelope_schema_version', 'evaluation_id', 'change_name', 'terminal_status', 'source_schema_version', 'sealed_at', 'baseline_sha256', 'evaluation_sha256', 'baseline', 'evaluation'], 'Evaluation history envelope');
            const terminal = Date.parse(record.sealed_at);
            const baselineTerminal = Date.parse(record.terminal_status === 'complete' ? record.baseline?.completed_at : record.baseline?.aborted_at);
            if (record.envelope_schema_version !== 1 || record.evaluation_id !== name.slice(0, -5) ||
                record.change_name !== change || !['complete', 'aborted'].includes(record.terminal_status) ||
                ![1, 2, 3].includes(record.source_schema_version) || !validTime(record.sealed_at) ||
                terminal !== baselineTerminal || terminalTimes.has(terminal) ||
                record.baseline_sha256 !== sha(Buffer.from(canonical(record.baseline))) ||
                record.baseline?.evaluation_id !== record.evaluation_id || record.baseline?.change_name !== change ||
                record.baseline?.status !== record.terminal_status || record.baseline?.schema_version !== record.source_schema_version) {
                fail('Evaluation history baseline identity');
            }
            terminalTimes.set(terminal, record.evaluation_id);
            if (record.terminal_status === 'complete') {
                if (!record.evaluation || record.evaluation.evaluation_id !== record.evaluation_id ||
                    record.evaluation.change_name !== change ||
                    record.evaluation_sha256 !== sha(Buffer.from(canonical(record.evaluation))) ||
                    !validTime(record.evaluation.evaluated_at) || Date.parse(record.evaluation.evaluated_at) > terminal) {
                    fail('Evaluation history payload identity');
                }
                if ([2, 3].includes(record.evaluation.schema_version) && record.evaluation_id !== evaluation.evaluation_id) {
                    priorEvaluations.push({ evaluation: record.evaluation, terminal });
                }
            } else if (record.evaluation !== null || record.evaluation_sha256 !== null) {
                fail('aborted Evaluation history payload');
            }
        }
    }
    priorEvaluations.sort((a, b) => a.terminal - b.terminal);
    const previousEvaluation = priorEvaluations.at(-1)?.evaluation;
    for (const previousFinding of previousEvaluation?.change_review?.findings || []) {
        if (!['Open', 'Deferred'].includes(previousFinding.status)) continue;
        const currentFinding = findingMap.get(previousFinding.id);
        if (!currentFinding || currentFinding.stage !== previousFinding.stage ||
            currentFinding.category !== previousFinding.category || currentFinding.severity !== previousFinding.severity ||
            currentFinding.summary !== previousFinding.summary || currentFinding.return_to !== previousFinding.return_to ||
            previousFinding.status === 'Open' && !['Open', 'Resolved'].includes(currentFinding.status) ||
            previousFinding.status === 'Deferred' && !['Deferred', 'Resolved'].includes(currentFinding.status)) {
            fail('unclosed prior finding disappeared or changed: ' + previousFinding.id);
        }
    }
    const stageMap = new Map;
    for (let index = 0; index < review.stages.length; index++) {
        const stage = review.stages[index], name = stageNames[index];
        closedObject(stage, ['name', 'started_at', 'completed_at', 'status', 'requirement_refs', 'task_ids', 'reviewed_paths', 'dimensions', 'evidence_command_ids', 'finding_ids', 'blocking_untested_ids', 'not_run_reason'], 'review stage');
        if (stage.name !== name || stageMap.has(name) || !['Pass', 'Fail', 'Blocked', 'NotRun'].includes(stage.status)) fail('review stage identity');
        stageMap.set(name, stage);
        if (stage.status === 'NotRun') {
            if (stage.started_at !== null || stage.completed_at !== null || stage.requirement_refs.length || stage.task_ids.length ||
                stage.reviewed_paths.length || stage.dimensions.length || stage.evidence_command_ids.length ||
                stage.finding_ids.length || stage.blocking_untested_ids.length ||
                typeof stage.not_run_reason !== 'string' || !stage.not_run_reason.trim()) fail('NotRun review stage contract');
            continue;
        }
        if (!validTime(stage.started_at) || !validTime(stage.completed_at) ||
            Date.parse(stage.started_at) < Date.parse(baseline.started_at) ||
            Date.parse(stage.completed_at) < Date.parse(stage.started_at) ||
            Date.parse(stage.completed_at) > Date.parse(evaluation.evaluated_at) || stage.not_run_reason !== null) fail('review stage time');
        exact(stage.dimensions, dimensions[name], name + ' dimensions');
        exact(stage.requirement_refs.map(refKey), [...deltaUniverse], name + ' requirement refs');
        exact(stage.task_ids, [...tasks.keys()], name + ' task IDs');
        exact(stage.reviewed_paths, reviewPaths, name + ' review paths');
        useCommands(stage.evidence_command_ids, name, stage.status === 'Pass');
        if (!stage.evidence_command_ids.length) fail(name + ' lacks command evidence');
        if (!Array.isArray(stage.blocking_untested_ids) || stage.blocking_untested_ids.some(id => !blocking.has(id))) fail(name + ' blocking references');
        stage.blocking_untested_ids.forEach(id => usedBlocking.add(id));
        exact(stage.finding_ids, findingsByStage.get(name).map(item => item.id), name + ' finding IDs');
        const open = findingsByStage.get(name).filter(item => item.status === 'Open' && ['Critical', 'Important'].includes(item.severity));
        const plannerIds = new Set(integration.planner_blocking_finding_ids);
        const expected = open.some(item => !plannerIds.has(item.id)) ? 'Fail' :
            open.some(item => plannerIds.has(item.id)) || stage.blocking_untested_ids.length ? 'Blocked' : 'Pass';
        if (stage.status !== expected) fail(name + ' aggregate mismatch');
    }
    const specification = stageMap.get('specification_compliance'), quality = stageMap.get('code_quality');
    if (specification.status !== 'NotRun' && quality.status !== 'NotRun' &&
        Date.parse(specification.completed_at) > Date.parse(quality.started_at)) fail('review stage order');
    if (['Fail', 'Blocked'].includes(specification.status) && quality.status !== 'NotRun') fail('quality review must not run after blocked specification review');
    const reviewResult = specification.status === 'Fail' || quality.status === 'Fail' ? 'Fail' :
        specification.status !== 'Pass' || quality.status !== 'Pass' ? 'Blocked' : 'Pass';

    const criterionIds = new Set, coveredTasks = new Set, coveredRefs = new Set;
    let behaviorResult = 'Pass';
    if (!Array.isArray(evaluation.criteria) || !evaluation.criteria.length) fail('criteria are required');
    for (const criterion of evaluation.criteria) {
        closedObject(criterion, ['id', 'description', 'requirement_refs', 'task_ids', 'status', 'evidence_command_ids', 'blocking_untested_ids'], 'criterion');
        if (typeof criterion.id !== 'string' || !criterion.id || criterionIds.has(criterion.id) ||
            typeof criterion.description !== 'string' || !criterion.description.trim() ||
            !Array.isArray(criterion.requirement_refs) || !criterion.requirement_refs.length ||
            !Array.isArray(criterion.task_ids) || !criterion.task_ids.length ||
            criterion.task_ids.some(id => !tasks.has(id)) ||
            !Array.isArray(criterion.blocking_untested_ids) ||
            criterion.blocking_untested_ids.some(id => !blocking.has(id))) fail('criterion contract');
        criterionIds.add(criterion.id);
        const refs = criterion.requirement_refs.map(refKey);
        if (refs.some(key => !deltaUniverse.has(key))) fail('criterion ref outside delta universe');
        refs.forEach(key => coveredRefs.add(key)); criterion.task_ids.forEach(id => {
            coveredTasks.add(id);
            if (!tasks.get(id).refKeys.some(key => refs.includes(key))) fail('criterion task/ref traceability');
        });
        useCommands(criterion.evidence_command_ids, 'criterion', criterion.status === 'Pass');
        criterion.blocking_untested_ids.forEach(id => usedBlocking.add(id));
        if (criterion.status === 'Pass') {
            if (!criterion.evidence_command_ids.length || criterion.blocking_untested_ids.length) fail('Pass criterion evidence');
        } else if (criterion.status === 'Fail') {
            if (!criterion.evidence_command_ids.length) fail('Fail criterion evidence');
            behaviorResult = 'Fail';
        } else if (criterion.status === 'Blocked') {
            if (!criterion.blocking_untested_ids.length) fail('Blocked criterion evidence');
            if (behaviorResult !== 'Fail') behaviorResult = 'Blocked';
        } else fail('criterion status');
    }
    exact([...coveredTasks], [...tasks.keys()], 'criterion task coverage');
    exact([...coveredRefs], [...deltaUniverse], 'criterion delta coverage');
    for (const assessment of evaluation.integration_completeness.surface_assessments) assessment.blocking_untested_ids.forEach(id => usedBlocking.add(id));
    for (const orphan of evaluation.integration_completeness.orphan_surfaces) orphan.blocking_untested_ids.forEach(id => usedBlocking.add(id));
    exact([...usedBlocking], [...blocking.keys()], 'blocking item usage');

    const eco = evaluation.implementation_economy;
    closedObject(eco, ['footprint_status', 'drift_explanation', 'classification_assessment', 'repository_impact_assessment', 'reuse_assessments', 'structural_assessments', 'obsolete_item_assessments', 'exception_assessments', 'result'], 'Implementation Economy');
    if (eco.footprint_status !== footprint.status) fail('economy footprint status drift');
    const evidenceAssessment = (assessment, label, withId = true) => {
        const fields = withId ? ['id', 'result', 'reason', 'evidence_paths', 'evidence_command_ids'] : ['result', 'reason', 'evidence_paths', 'evidence_command_ids'];
        closedObject(assessment, fields, label, fields.filter(key => key !== 'evidence_command_ids'));
        if (withId && (typeof assessment.id !== 'string' || !assessment.id)) fail(label + ' ID');
        result(assessment.result, label); if (typeof assessment.reason !== 'string' || !assessment.reason.trim()) fail(label + ' reason');
        const commandIds = assessment.evidence_command_ids || [];
        if (!Array.isArray(assessment.evidence_paths) || !Array.isArray(commandIds) ||
            !assessment.evidence_paths.length && !commandIds.length) fail(label + ' evidence');
        assessment.evidence_paths.forEach(value => safeRepositoryPath(value, label + ' path'));
        useCommands(commandIds, label, assessment.result === 'Pass');
    };
    evidenceAssessment(eco.classification_assessment, 'classification assessment', false);
    exact(eco.classification_assessment.evidence_paths, implementationPaths, 'classification paths');
    closedObject(eco.repository_impact_assessment, ['result', 'surfaces'], 'repository impact');
    result(eco.repository_impact_assessment.result, 'repository impact');
    const repositorySurfaceNames = ['product_targets', 'install', 'package', 'ci'];
    if (!Array.isArray(eco.repository_impact_assessment.surfaces)) fail('repository surfaces');
    exact(eco.repository_impact_assessment.surfaces.map(item => item.surface), repositorySurfaceNames, 'repository surfaces');
    let applicable = 0; const repositoryResults = [];
    for (const surface of eco.repository_impact_assessment.surfaces) {
        closedObject(surface, ['surface', 'applicability', 'result', 'reason', 'evidence_paths', 'evidence_command_ids', 'not_applicable_reason'], 'repository surface');
        if (typeof surface.reason !== 'string' || !surface.reason.trim() || !Array.isArray(surface.evidence_paths) || !Array.isArray(surface.evidence_command_ids)) fail('repository surface evidence');
        if (surface.applicability === 'applicable') {
            applicable++; repositoryResults.push(result(surface.result, 'repository surface'));
            if (surface.not_applicable_reason !== null || !surface.evidence_paths.length && !surface.evidence_command_ids.length) fail('applicable repository surface evidence');
            surface.evidence_paths.forEach(value => safeRepositoryPath(value, 'repository surface path'));
            useCommands(surface.evidence_command_ids, 'repository surface', surface.result === 'Pass');
        } else if (surface.applicability === 'not_applicable') {
            if (surface.result !== null || surface.evidence_paths.length || surface.evidence_command_ids.length ||
                typeof surface.not_applicable_reason !== 'string' || !surface.not_applicable_reason.trim()) fail('not-applicable repository surface');
        } else fail('repository surface applicability');
    }
    const repositoryResult = !applicable ? 'Blocked' : repositoryResults.includes('Fail') ? 'Fail' : repositoryResults.includes('Blocked') ? 'Blocked' : 'Pass';
    if (eco.repository_impact_assessment.result !== repositoryResult) fail('repository impact aggregate');
    const assessmentGroups = [
        ['reuse', eco.reuse_assessments, (economyPlan.reuse_decisions || []).map(item => item.id)],
        ['obsolete', eco.obsolete_item_assessments, (economyPlan.obsolete_items || []).map(item => item.id)],
        ['exception', eco.exception_assessments, (economyPlan.exceptions || []).map(item => item.id)]
    ];
    for (const [label, assessments, expectedIds] of assessmentGroups) {
        if (!Array.isArray(assessments)) fail(label + ' assessments');
        assessments.forEach(item => evidenceAssessment(item, label + ' assessment'));
        exact(assessments.map(item => item.id), expectedIds, label + ' assessment IDs');
    }
    if (!Array.isArray(eco.structural_assessments)) fail('structural assessments');
    const allowanceKinds = new Map, allowanceIds = [];
    for (const [kind, entries] of Object.entries(economyPlan.structural_allowances || {})) for (const entry of entries || []) { allowanceKinds.set(entry.id, kind); allowanceIds.push(entry.id); }
    const candidateMap = new Map((footprint.structural_candidates || []).map(item => [item.candidate_id, item]));
    const seenAllowances = [], seenCandidates = [];
    for (const assessment of eco.structural_assessments) {
        closedObject(assessment, ['allowance_id', 'candidate_ids', 'result', 'reason', 'evidence_paths', 'evidence_command_ids'], 'structural assessment');
        if (assessment.allowance_id !== null) {
            if (!allowanceKinds.has(assessment.allowance_id)) fail('unknown structural allowance');
            seenAllowances.push(assessment.allowance_id);
        }
        if (!Array.isArray(assessment.candidate_ids) || assessment.candidate_ids.some(id => !candidateMap.has(id))) fail('structural candidate IDs');
        if (assessment.allowance_id !== null && assessment.candidate_ids.some(id => candidateMap.get(id).allowance_kind !== allowanceKinds.get(assessment.allowance_id))) fail('structural allowance kind mismatch');
        seenCandidates.push(...assessment.candidate_ids);
        result(assessment.result, 'structural assessment');
        if (typeof assessment.reason !== 'string' || !assessment.reason.trim() ||
            !Array.isArray(assessment.evidence_paths) || !Array.isArray(assessment.evidence_command_ids) ||
            !assessment.evidence_paths.length && !assessment.evidence_command_ids.length ||
            evaluation.verdict === 'Pass' && assessment.allowance_id === null) fail('structural assessment evidence');
        assessment.evidence_paths.forEach(value => safeRepositoryPath(value, 'structural assessment path'));
        useCommands(assessment.evidence_command_ids, 'structural assessment', assessment.result === 'Pass');
    }
    exact(seenAllowances, allowanceIds, 'structural allowance IDs');
    exact(seenCandidates, [...candidateMap.keys()], 'structural candidate IDs');
    if (eco.footprint_status === 'within_expected') {
        if (eco.drift_explanation !== null) fail('unexpected economy drift explanation');
    } else {
        closedObject(eco.drift_explanation, ['metric_keys', 'reason', 'why_no_replan'], 'economy drift explanation');
        if (!Array.isArray(eco.drift_explanation.metric_keys) || !eco.drift_explanation.metric_keys.length ||
            typeof eco.drift_explanation.reason !== 'string' || !eco.drift_explanation.reason.trim() ||
            typeof eco.drift_explanation.why_no_replan !== 'string' || !eco.drift_explanation.why_no_replan.trim()) fail('economy drift explanation contract');
        const values = { production: footprint.production, tests: footprint.tests, project_support: footprint.project_support, generated: footprint.generated }, expected = [];
        for (const [group, metrics] of Object.entries(economyPlan.thresholds || {})) for (const [metric, threshold] of Object.entries(metrics || {})) {
            if (values[group]?.[metric] > threshold.expected && values[group]?.[metric] < threshold.review_at) expected.push(group + '.' + metric);
        }
        exact(eco.drift_explanation.metric_keys, expected, 'economy drift metrics');
    }
    const economyComponents = [eco.classification_assessment.result, repositoryResult, ...eco.reuse_assessments.map(item => item.result),
        ...eco.structural_assessments.map(item => item.result), ...eco.obsolete_item_assessments.map(item => item.result), ...eco.exception_assessments.map(item => item.result)];
    const economyResult = economyComponents.includes('Fail') ? 'Fail' : economyComponents.includes('Blocked') ? 'Blocked' : economyComponents.every(value => value === 'Pass') ? 'Pass' : 'Blocked';
    if (eco.result !== economyResult) fail('Implementation Economy aggregate');
    exact([...usedCommands], [...commands.keys()], 'Evaluation command usage');
    const verdict = [behaviorResult, reviewResult, economyResult, integration.result].includes('Fail') ? 'Fail' :
        [behaviorResult, reviewResult, economyResult, integration.result].includes('Blocked') ? 'Blocked' : 'Pass';
    if (evaluation.verdict !== verdict) fail('unified verdict aggregate');
    if (verdict === 'Pass' && (evaluation.blocking_untested.length || !evaluation.commands.some(command => command.result === 'Pass' && command.kind !== 'static'))) fail('Pass lacks executable evidence');

    if (baseline.status === 'complete') {
        const envelopeRecord = readCanonical(path.join(harness, 'evaluations', baseline.evaluation_id + '.json'), 'terminal Evaluation envelope');
        const envelope = envelopeRecord.value;
        closedObject(envelope, ['envelope_schema_version', 'evaluation_id', 'change_name', 'terminal_status', 'source_schema_version', 'sealed_at', 'baseline_sha256', 'evaluation_sha256', 'baseline', 'evaluation'], 'terminal Evaluation envelope');
        if (envelope.envelope_schema_version !== 1 || envelope.evaluation_id !== baseline.evaluation_id ||
            envelope.change_name !== change || envelope.terminal_status !== 'complete' || envelope.source_schema_version !== 3 ||
            envelope.sealed_at !== baseline.completed_at || envelope.baseline_sha256 !== sha(Buffer.from(canonical(baseline))) ||
            envelope.evaluation_sha256 !== sha(Buffer.from(canonical(evaluation))) ||
            canonical(envelope.baseline) !== canonical(baseline) || canonical(envelope.evaluation) !== canonical(evaluation)) fail('terminal Evaluation envelope drift');
    }
    return { result: verdict, source_fingerprint: sourceFingerprint, artifact_fingerprint: artifactFingerprint, integration };
}

function reportBytes(report) { return Buffer.from(JSON.stringify(report, null, 2) + '\n'); }
function diagnostic(change, status, reason) { return { schema_version: 1, change_name: change, status, reason: String(reason || 'unknown') }; }

module.exports = { parsePlan, parsePlanFromChangeRoot, parseEconomy, parseTasks, parseDeltaUniverse, normalizeSymbolIdentity, normalizeSurfaceRoles, bindSurfaceProbes, expectedTaskSurfaceIds, verifyIntegrationEvidence, verifyIntegrationEvidenceFromChangeRoot, buildReviewedReport, validateEvaluationV3, validateCompleteEvaluationV3, discoveryIdentity, reportBytes, diagnostic, requiredRoles, currentRegularFile, canonical, sha, DIGEST, KINDS, VERIFY_KINDS, ROLE_ORDER };

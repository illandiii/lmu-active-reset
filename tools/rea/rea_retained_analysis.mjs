// Only recover a completed analysis that REA explicitly retained beyond MCP's budget.
export function retainedAnalysisId(response) {
  if (!response?.isError) return null;
  for (const block of response.content ?? []) {
    if (block.type !== 'text') continue;
    let error;
    try { error = JSON.parse(block.text).error; } catch { continue; }
    const limits = error?.details?.reported_limits;
    if (error?.code === 'resource_constraint' && error.details?.operation === 'analyze_function' &&
        error.details?.resource === 'transport' && limits?.boundary === 'mcp-response' &&
        limits.evidence_reference?.kind === 'retained-evidence' &&
        limits.evidence_id === limits.evidence_reference.evidence_id && /^ev_[0-9a-f]{64}$/.test(limits.evidence_id)) return limits.evidence_id;
  }
  return null;
}
export function recoverAnalysis(snapshot, evidenceId) {
  const records = snapshot?.evidence_bundle?.records;
  const matches = Array.isArray(records) ? records.filter(record => record.evidence_id === evidenceId) : [];
  if (matches.length !== 1 || matches[0].operation !== 'analyze_function' || !matches[0].raw_result) {
    throw new Error(`Missing or ambiguous retained analysis: ${evidenceId}`);
  }
  return matches[0];
}

// Build-time literal matching only; not an HTML sanitizer or markup renderer.
// One pass never decodes an entity newly produced by another replacement.
export function decodeHtmlSourceText(source){
  const entities={'&amp;':'&','&lt;':'<','&gt;':'>'};
  return source.replace(/&(?:amp|lt|gt);/g,entity=>entities[entity]);
}

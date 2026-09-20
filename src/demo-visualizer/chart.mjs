import {msDelta} from './protocol.mjs';
export function drawTrace(canvas, trace) {
  const dpr = window.devicePixelRatio || 1, width = canvas.clientWidth, height = canvas.clientHeight;
  canvas.width = Math.max(1,Math.round(width*dpr)); canvas.height = Math.max(1,Math.round(height*dpr));
  const c = canvas.getContext('2d'); c.scale(dpr,dpr); c.clearRect(0,0,width,height);
  const m = {left: 66, right: 18, top: 28, bottom: 31};
  const w = Math.max(1,width-m.left-m.right), h = Math.max(1,height-m.top-m.bottom);
  const points = trace?.points ?? [];
  const values = points.map(p => p.raw-(trace.base ?? 0));
  let low = points.length ? Math.min(...values) : -1, high = points.length ? Math.max(...values) : 1;
  const pad = Math.max((high-low)*.14,1); low -= pad; high += pad;
  const x0 = points.length ? points[0].t : -1, x1 = points.length ? Math.max(points.at(-1).t,x0+.1) : 3;
  const x = t => m.left+(t-x0)/(x1-x0)*w, y = v => m.top+(high-v)/(high-low)*h;
  c.font = '10px "Segoe UI", sans-serif'; c.lineWidth = 1;
  for (let i=0;i<=4;i++) {
    const yy = m.top+h*i/4, v = high-(high-low)*i/4;
    c.strokeStyle = '#edf0e9'; c.beginPath(); c.moveTo(m.left,yy); c.lineTo(width-m.right,yy); c.stroke();
    c.fillStyle = '#94a18e'; c.textAlign = 'right';
    if (points.length) c.fillText(Math.round(v/1000).toLocaleString()+'k',m.left-10,yy+3);
    const t = x0+(x1-x0)*i/4;
    c.textAlign = 'center'; c.fillText(t.toFixed(1)+'s',x(t),height-10);
  }
  if (!points.length) return;
  if (trace.base !== null && low < 0 && high > 0) {
    c.strokeStyle = '#a4b59a'; c.setLineDash([3,4]); c.beginPath(); c.moveTo(m.left,y(0)); c.lineTo(width-m.right,y(0)); c.stroke(); c.setLineDash([]);
  }
  c.beginPath(); points.forEach((p,i) => i ? c.lineTo(x(p.t),y(values[i])) : c.moveTo(x(p.t),y(values[i])));
  c.strokeStyle = '#397857'; c.lineWidth = 2; c.lineJoin='round'; c.stroke();
  const mark = (time,label,color) => {
    if (time < x0 || time > x1) return;
    c.strokeStyle=color; c.setLineDash([3,4]); c.lineWidth=1;
    c.beginPath(); c.moveTo(x(time),m.top); c.lineTo(x(time),m.top+h); c.stroke(); c.setLineDash([]);
    c.fillStyle=color; c.textAlign='center'; c.fillText(label,Math.min(width-50,Math.max(90,x(time))),14);
  };
  if (trace.exactBounds) { mark(0,'押下検出','#b7912d'); mark(msDelta(trace.end,trace.trigger)/1000,'波形取得完了','#bf6a60'); }
}

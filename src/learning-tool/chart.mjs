// Raw counts plotted against MCU time; discontinuities break the trace.
export function drawChart(canvas, points) {
  const ctx = canvas.getContext('2d');
  const width = canvas.clientWidth, height = canvas.clientHeight, scale = devicePixelRatio || 1;
  canvas.width = Math.round(width * scale); canvas.height = Math.round(height * scale); ctx.scale(scale, scale);
  ctx.strokeStyle = '#e5ece6'; ctx.lineWidth = 1;
  for (let i = 0; i <= 4; ++i) {
    const y = 14 + (height - 28) * i / 4;
    ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(width, y); ctx.stroke();
  }
  if (!points.length) {
    ctx.fillStyle = '#809087'; ctx.font = '14px sans-serif'; ctx.textAlign = 'center';
    ctx.fillText('まだデータはありません', width / 2, height / 2);
    return 'センサーを接続すると波形を表示します';
  }
  const minimum = Math.min(...points.map(p => p.raw)), maximum = Math.max(...points.map(p => p.raw));
  const span = Math.max(1, maximum - minimum), start = points[0].elapsed;
  const duration = Math.max(1, points.at(-1).elapsed - start);
  ctx.strokeStyle = '#24846b'; ctx.lineWidth = 2; ctx.beginPath();
  points.forEach((point, i) => {
    const x = (point.elapsed - start) / duration * width;
    const y = maximum === minimum ? height / 2 : height - 20 - (point.raw - minimum) / span * (height - 40);
    if (i === 0 || point.gap) ctx.moveTo(x, y); else ctx.lineTo(x, y);
  });
  ctx.stroke();
  return `${minimum.toLocaleString()} ～ ${maximum.toLocaleString()} counts · ${(duration / 1000).toFixed(1)} 秒`;
}

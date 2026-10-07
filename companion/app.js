const connectBtn = document.getElementById("connect");
const statusEl = document.getElementById("status");
const rowsEl = document.getElementById("rows");
const barsEl = document.getElementById("bars");
const ssids = new Map();

function setStatus(text) {
  statusEl.textContent = text;
}

function renderBars(counts) {
  if (!Array.isArray(counts) || !counts.length) return;
  const max = Math.max(1, ...counts);
  barsEl.replaceChildren();
  counts.forEach((n, i) => {
    const col = document.createElement("div");
    const bar = document.createElement("i");
    bar.style.height = `${Math.max(2, Math.round((n / max) * 72))}px`;
    const label = document.createElement("span");
    label.textContent = String(i + 1);
    col.appendChild(bar);
    col.appendChild(label);
    barsEl.appendChild(col);
  });
}

function render(obj) {
  document.getElementById("ch").textContent = obj.ch ?? "–";
  document.getElementById("rate").textContent = obj.mgmt_rate ?? "–";
  document.getElementById("beacons").textContent = obj.beacons ?? "–";
  document.getElementById("probes").textContent = obj.probes ?? "–";
  document.getElementById("stations").textContent = obj.stations ?? "–";
  renderBars(obj.ch_counts);
  for (const row of obj.ssids || []) {
    ssids.set(row.bss || row.ssid || "?", row);
  }
  const sorted = [...ssids.values()].sort((a, b) => (b.rssi || -127) - (a.rssi || -127));
  rowsEl.replaceChildren();
  for (const row of sorted) {
    const tr = document.createElement("tr");
    const flags = [row.hidden ? "hidden" : "", row.ht ? "ht" : ""].filter(Boolean).join(", ");
    for (const cell of [row.ssid || "(hidden)", row.ch, row.rssi, flags]) {
      const td = document.createElement("td");
      td.textContent = cell;
      tr.appendChild(td);
    }
    rowsEl.appendChild(tr);
  }
}

async function readLoop(port) {
  const reader = port.readable.getReader();
  const decoder = new TextDecoder();
  let buf = "";
  setStatus("Listening.");
  try {
    while (true) {
      const { value, done } = await reader.read();
      if (done) break;
      buf += decoder.decode(value, { stream: true });
      let nl;
      while ((nl = buf.indexOf("\n")) >= 0) {
        const line = buf.slice(0, nl).trim();
        buf = buf.slice(nl + 1);
        if (!line.startsWith("{")) continue;
        try {
          const obj = JSON.parse(line);
          if (obj.t === "hello") setStatus(`Board up · ${obj.fw} · dwell ${obj.dwell_ms} ms`);
          if (obj.t === "census") render(obj);
        } catch (_) {}
      }
    }
  } finally {
    reader.releaseLock();
    setStatus("Disconnected.");
    connectBtn.disabled = false;
  }
}

connectBtn.addEventListener("click", async () => {
  if (!("serial" in navigator)) {
    setStatus("Web Serial needs Chrome or Edge.");
    return;
  }
  try {
    const port = await navigator.serial.requestPort();
    await port.open({ baudRate: 115200 });
    connectBtn.disabled = true;
    readLoop(port);
  } catch (err) {
    setStatus(err && err.message ? err.message : "Connect cancelled.");
  }
});


var systemDataVersion = -1;
var consStats = null;

async function fetchAPI(uri, cb) {

  var response;
  var data;
  try {
    response = await fetch(uri);
    if (!response.ok) {
      throw new Error(`HTTP error! Status: ${response}`);
    }
    data = await response.json();
    Object.keys(data).forEach(key => {
      document.getElementsByName(key).forEach(e => {
        if (e.type == "radio") {
          e.checked = e.value == data[key];
        } else if (e instanceof HTMLInputElement) {
          e.value = data[key];
        } else {
          const dv = "data-value";
          var v = data[key];
          if (typeof (v) === "boolean") {
            e.textContent = v ? "On" : "Off";
            e.setAttribute(dv, v);
          } else if (Array.isArray(v) && v.length) {
            e.textContent = toLogEntry(v[0]);
          } else {
            if (e.hasAttribute(dv)) {
              e.setAttribute(dv, v);
            } else {
              e.textContent = v;
            }
          }
        }
      });
    });
  } catch (error) {
    console.error('Fetch error:', error);
  }
  cb !== undefined && cb(response, data);
}

function fetchData() {
  fetchAPI("/api/data", function (response, json) {
    if (response !== undefined && response.ok && json !== undefined) {
      systemDataVersion = json["version"];
      toggleLoadPowerFields(false);
      if (json["cons_stats"]) consStats = json["cons_stats"];
    }
  });
}

async function fetchStatus() {
  await fetchAPI("/api/status", function (response, json) {
    if (response !== undefined && response.ok && json !== undefined) {
      updateBattery(json["usoc"]);
      const events = json["events"];
      if (events !== undefined && events.length) {
        eventLog.addEvents(events);
      }
      const errors = json["errors"];
      if (errors !== undefined) {
        renderLog(document.getElementById("errorLog"), errors, "li");
        document.getElementById("errors").style.display = errors.length ? "block" : "none";
      }
      if (systemDataVersion !== json["version"]) {
        fetchData();
      }
    }
    setTimeout(fetchStatus, 3000);
  });
}

function updateBattery(level) {
  const chargeBar = document.getElementById("charge");

  if (level !== "") {
    var lvl = "low";
    chargeBar.style.height = level + "%";
    if (level > 60) {
      lvl = "high";
    } else if (level > 10) {
      lvl = "medium";
    }
    chargeBar.setAttribute("data-lvl", lvl);
    document.getElementById("charge-lvl").textContent = level + "%";
  }
}

class EventLog {
  events = [];

  addEvents(eLs) {
    this.events = eLs;
  }
}

const eventLog = new EventLog(10);

function toLogEntry(e) {
  return new Date(e.ts * 1000).toLocaleString() + " - " + e.msg;
}

function renderLog(el, ls, nm) {
  el.innerHTML = "";
  ls.forEach(e => {
    const t = document.createElement(nm);
    t.textContent = toLogEntry(e);
    el.appendChild(t);
  });
}

function batteryInfo() {
  const overlay = document.getElementById("batInfoOverlay");
  overlay.classList.add("active");
}

function showLog() {
  const overlay = document.getElementById("logOverlay");
  const logElem = document.getElementById("eventLog");
  if (logElem !== null) {
    renderLog(logElem, eventLog.events, "div");
    overlay.classList.add("active");
  }
}

function closeOverlay(btn) {
  btn.closest('.overlay').classList.remove('active');
}

function showConsStats() {
  document.getElementById("statsOverlay").classList.add("active");
  renderConsStats(new Date().getDay());
}

function renderConsStats(dayIdx) {
  document.querySelectorAll('#statsOverlay .day-btn').forEach((btn, i) => {
    btn.classList.toggle('active', i === dayIdx);
  });
  if (!consStats) return;
  const data = consStats[dayIdx];
  const maxVal = Math.max(...consStats.flat(), 1);
  const yAxis = document.getElementById("statsYAxis");
  if (yAxis) {
    yAxis.innerHTML = "";
    [maxVal, Math.round(maxVal / 2), 0].forEach(v => {
      const s = document.createElement("span"); s.textContent = (v + " Wh"); yAxis.appendChild(s);
    });
  }
  const bars = document.getElementById("statsBars");
  bars.innerHTML = "";
  data.forEach((v, h) => {
    const col = document.createElement("div");
    col.className = "bar-col";
    const bar = document.createElement("div");
    bar.className = "bar" + (h === new Date().getHours() && dayIdx === new Date().getDay() ? " bar-now" : "");
    bar.style.height = Math.round(v / maxVal * 100) + "%";
    bar.title = h + ":00 — " + v + " Wh";
    col.appendChild(bar);
    bars.appendChild(col);
  });
  const xAxis = document.getElementById("statsXAxis");
  if (xAxis && !xAxis.childElementCount) {
    for (let h = 0; h < 24; h++) {
      const lbl = document.createElement("div");
      lbl.className = "bar-lbl";
      lbl.textContent = (h % 6 === 0) ? h : "";
      xAxis.appendChild(lbl);
    }
  }
}

function sendUpdate(payload) {
  fetch("/api/update", {
    method: "POST",
    headers: {
      "content-type": "application/x-www-form-urlencoded"
    },
    body: new URLSearchParams(payload).toString()
  });
}

function toggleLoadPowerFields(isLoading) {
  document.getElementById("sn_loadpower").disabled = isLoading;
  const btn = document.getElementById("calibrateBtn");
  btn.disabled = isLoading;
  if (isLoading) {
    btn.classList.add("loading");
  } else {
    btn.classList.remove("loading");
  }
}

function triggerCalibrate() {
  toggleLoadPowerFields(true);
  sendUpdate("calibrate");
}

document.querySelectorAll("input[type='radio']").forEach(radio => {
  radio.addEventListener("change", (event) => {
    const form = radio.closest("form");
    const formData = new FormData(form);
    sendUpdate(formData);
  });
});

fetchStatus();
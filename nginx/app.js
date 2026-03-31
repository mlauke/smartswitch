
var systemDataVersion = -1;
var consStats = null;
var pvForecastByDayHour = null; // {dayOfWeek: {hour: wh}}

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
        const v = data[key];
        if (e.type == "radio") {
          e.checked = e.value == v;
        } else if (e instanceof HTMLInputElement) {
          e.value = v;
        } else {
          const dv = "data-value";
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
      if (json["pv_forecast"]) pvForecastByDayHour = parsePvForecast(json["pv_forecast"]);
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

function parsePvForecast(raw) {
  const map = {};
  raw.forEach(([ts, wh]) => {
    const d = new Date(ts * 1000);
    const day = d.getDay();
    const hour = d.getHours();
    if (!map[day]) map[day] = {};
    map[day][hour] = wh;
  });
  return map;
}

function renderConsStats(dayIdx) {
  document.querySelectorAll('#statsOverlay .day-btn').forEach((btn, i) => {
    btn.classList.toggle('active', i === dayIdx);
  });
  if (!consStats) return;
  const data = consStats[dayIdx];
  const forecastDay = pvForecastByDayHour ? (pvForecastByDayHour[dayIdx] || {}) : {};
  const forecastVals = Object.values(forecastDay);
  const maxVal = Math.max(...consStats.flat(), ...forecastVals, 1);
  const yAxis = document.getElementById("statsYAxis");
  if (yAxis) {
    yAxis.innerHTML = "";
    [maxVal, Math.round(maxVal / 2), 0].forEach(v => {
      const s = document.createElement("span"); s.textContent = (v + " Wh"); yAxis.appendChild(s);
    });
  }
  const bars = document.getElementById("statsBars");
  bars.innerHTML = "";
  const currentDay = new Date().getDay();
  const currentHour = new Date().getHours()
  const currentAvg = document.getElementById("cons_avg_w").getAttribute("data-value");
  data.forEach((v, h) => {
    const col = document.createElement("div");
    col.className = "bar-col";
    const bar = document.createElement("div");
    bar.className = "bar";
    if (h === currentHour && dayIdx === currentDay) {
      bar.className += " bar-now";
      v = currentAvg;
    }
    bar.style.height = Math.round(v / maxVal * 100) + "%";
    bar.title = h + ":00 — " + v + " Wh";
    col.appendChild(bar);
    const fWh = forecastDay[h] || 0;
    if (fWh > 0) {
      const fBar = document.createElement("div");
      fBar.className = "bar-forecast";
      fBar.style.height = Math.round(fWh / maxVal * 100) + "%";
      fBar.title = h + ":00 — " + fWh + " Wh (Forecast)";
      col.appendChild(fBar);
    }
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
async function fetchData(uri, cb) {

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
          var v = data[key];
          if (typeof (v) === "boolean") {
            e.textContent = v ? "On" : "Off";
            e.setAttribute("data-value", v);
          } else if (Array.isArray(v) && v.length) {
            e.textContent = toLogEntry(v[0]);
          } else {
            if (e.hasAttribute("data-value")) {
              e.setAttribute("data-value", v);
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

function fetchStatus() {
  fetchData("/api/status", function (response, json) {
    if (response !== undefined && response.ok && json !== undefined) {
      updateBattery();
      const events = json["events"];
      if (events !== undefined && events.length) {
        eventLog.addEvents(events);
      }
      const errors = json["errors"];
      if (errors !== undefined) {
        renderLog(document.getElementById("errorLog"), errors, "li");
        document.getElementById("errors").style.display = errors.length ? "block" : "none";
      }
    }
    setTimeout(fetchStatus, 3000);
  });
}

function updateBattery() {
  const chargeBar = document.getElementById("charge");
  const level = document.getElementById("usoc").value;

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

function showLog() {
  const overlay = document.getElementById("overlay");
  const logElem = document.getElementById("eventLog");
  if (logElem !== null) {
    renderLog(logElem, eventLog.events, "li");
    overlay.classList.add("active");
  }
}

function closeLog() {
  const logOverlay = document.getElementById("overlay");
  logOverlay !== null && logOverlay.classList.remove("active");
}

fetchData("/api/data");
fetchStatus();

document.querySelectorAll("input[type='radio']").forEach(radio => {
  radio.addEventListener("change", (event) => {
    const form = radio.closest("form");
    let formData = new FormData(form);
    fetch("/api/update", {
      method: "POST",
      headers: {
        "Content-Type": "application/x-www-form-urlencoded"
      },
      body: new URLSearchParams(formData).toString()
    });
  });
});

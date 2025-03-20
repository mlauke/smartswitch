async function fetchData(uri, cb) {

  var response;
  try {
    response = await fetch(uri);
    if (!response.ok) {
      throw new Error(`HTTP error! Status: ${response}`);
    }
    const data = await response.json();
    //data["events"] = new Date().toISOString() + ": " + "foo\n";
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
          } else {
            e.textContent = (e.hasAttribute("data-append") ? e.textContent + v : v);
          }
        }
      });
    });
  } catch (error) {
    console.error('Fetch error:', error);
  }
  cb !== undefined && cb(response);
}

function fetchStatus() {
  fetchData("/api/status", function (response) {
    if (response !== undefined && response.ok) {
      updateBattery();
    }
    setTimeout(fetchStatus, 3000);
  });
}

function updateBattery() {
  const chargeBar = document.getElementById("charge");
  const level = document.getElementById("usoc").value;

  if (level !== "") {
    chargeBar.style.height = level + "%";
    // Farbe anpassen je nach Ladestand
    if (level > 60) {
      chargeBar.setAttribute("data-level", "high");
    } else if (level > 10) {
      chargeBar.setAttribute("data-level", "medium");
    } else {
      chargeBar.setAttribute("data-level", "low");
    }
    document.getElementById("charge-lvl").textContent = level + "%";
  }
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
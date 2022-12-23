import Factory from "./powar.js";

const $i = document.getElementById;
const $q = document.querySelector;

const canvas = $i("canvas");

let rom = await localforage.getItem("rom");
let eeprom = await localforage.getItem("eeprom");

if (!rom || !eeprom) {
  $q(".upload").classList.add("visible");
} else if (rom && eeprom) {
  $q(".start").classList.add("visible");
  $q(".reset").classList.add("visible");
}

async function start() {
  let rom = await localforage.getItem("rom");
  let eeprom = await localforage.getItem("eeprom");

  const Module = {
    canvas,
    preRun: () => {
      const { FS } = Module;
      FS.createDataFile("/", "rom.bin", rom, true, true);
      FS.createDataFile("/", "eeprom.bin", eeprom, true, true);
    },
  };

  $q(".start").classList.remove("visible");
  $q(".reset").classList.add("visible");

  await Factory(Module);
}

async function checkForFiles() {
  let rom = await localforage.getItem("rom");
  let eeprom = await localforage.getItem("eeprom");
  if (rom && eeprom) {
    $q(".upload").classList.remove("visible");
    $q(".start").classList.add("visible");
    $q(".reset").classList.add("visible");
  }
}

async function readFileContent(file) {
  return new Promise((resolve) => {
    const reader = new FileReader();
    reader.addEventListener("load", (event) => {
      resolve(event.target.result);
    });
    reader.readAsArrayBuffer(file);
  });
}

const arrowLeftEvent = {
  code: "ArrowLeft",
  key: "ArrowLeft",
  keyCode: 37,
  which: 37,
};

const arrowDownEvent = {
  code: "ArrowDown",
  key: "ArrowDown",
  keyCode: 40,
  which: 40,
};

const arrowRightEvent = {
  code: "ArrowRight",
  key: "ArrowRight",
  keyCode: 39,
  which: 39,
};

$i("left").addEventListener("mousedown", () => {
  canvas.dispatchEvent(new KeyboardEvent("keydown", arrowLeftEvent));
});

$i("left").addEventListener("mouseup", () => {
  canvas.dispatchEvent(new KeyboardEvent("keyup", arrowLeftEvent));
});

$i("middle").addEventListener("mousedown", () => {
  canvas.dispatchEvent(new KeyboardEvent("keydown", arrowDownEvent));
});

$i("middle").addEventListener("mouseup", () => {
  canvas.dispatchEvent(new KeyboardEvent("keyup", arrowDownEvent));
});

$i("right").addEventListener("mousedown", () => {
  canvas.dispatchEvent(new KeyboardEvent("keydown", arrowRightEvent));
});

$i("right").addEventListener("mouseup", () => {
  canvas.dispatchEvent(new KeyboardEvent("keyup", arrowRightEvent));
});

$i("uploadRom").addEventListener("click", () => {
  $i("rom").click();
});

$i("rom").addEventListener("change", async (event) => {
  const files = event.target.files;
  const buffer = await readFileContent(files[0]);
  await localforage.setItem("rom", new Uint8Array(buffer));
  $i("uploadRom").textContent = files[0].name;
  await checkForFiles();
});

$i("eeprom").addEventListener("change", async (event) => {
  const files = event.target.files;
  const buffer = await readFileContent(files[0]);
  await localforage.setItem("eeprom", new Uint8Array(buffer));
  $i("uploadEeprom").textContent = files[0].name;
  await checkForFiles();
});

$i("uploadEeprom").addEventListener("click", () => {
  $i("eeprom").click();
});

$i("start").addEventListener("click", start);

$i("resetUploads").addEventListener("click", async () => {
  await localforage.removeItem("rom");
  await localforage.removeItem("eeprom");

  window.location.reload();
});

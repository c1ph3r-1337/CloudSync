#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>
#include <SD.h>
#include <WebSocketsServer.h>

// SD card chip select pin
#define SD_CS_PIN 5

// WiFi credentials
const char* ssid = "INFECTED NETWORK 2.4 GHz";
const char* password = "Jas@1234";
IPAddress local_IP(192, 168, 1, 100);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

// Create HTTP server on port 80
WebServer server(80);

// Create WebSocket server on port 81
WebSocketsServer webSocket = WebSocketsServer(81);

// Global file handle for uploads
File fsUploadFile;

// Typical Android folders to hide
static const char* EXCLUDED_FOLDERS[] = {
  "Music","Podcasts","Ringtones","Alarms","Notifications",
  "Pictures","Movies","Download","DCIM","Documents",
  "Audiobooks","Recordings","Android"
};

// ----------------------------------------------------------------
// HELPER: Recursively sum the sizes of all files on the SD card
uint64_t sumFileSizesRecursive(const char* path) {
  uint64_t total = 0;
  File root = SD.open(path);
  if (!root) return 0;
  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      total += sumFileSizesRecursive(file.name());
    } else {
      total += file.size();
    }
    file.close();
    file = root.openNextFile();
  }
  return total;
}

// ----------------------------------------------------------------
// HELPER: Recursively remove a directory (and all sub-files/folders)
bool removeDirectoryRecursive(const char * path) {
  File dir = SD.open(path);
  if (!dir) return false;
  File entry = dir.openNextFile();
  while (entry) {
    String entryPath = String(entry.name());
    if (entry.isDirectory()) {
      entry.close();
      removeDirectoryRecursive(entryPath.c_str());
    } else {
      SD.remove(entryPath);
      entry.close();
    }
    entry = dir.openNextFile();
  }
  dir.close();
  return SD.rmdir(path);
}

// ----------------------------------------------------------------
// HTML code
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8"/>
  <meta name="viewport" content="width=device-width, initial-scale=1.0"/>
  <title>CloudSync - My Files</title>
  <style>
    :root {
      --primary: #3b82f6;
      --primary-hover: #2563eb;
      --primary-foreground: #ffffff;
      --background: #f8fafc;
      --sidebar-bg: #ffffff;
      --sidebar-border: #e2e8f0;
      --card-bg: #ffffff;
      --foreground: #0f172a;
      --muted: #f1f5f9;
      --muted-foreground: #64748b;
      --border: #e2e8f0;
      --radius: 0.5rem;
      --shadow: 0 1px 3px rgba(0,0,0,0.1), 0 1px 2px rgba(0,0,0,0.06);
      --shadow-hover: 0 4px 6px -1px rgba(0,0,0,0.1), 0 2px 4px -1px rgba(0,0,0,0.06);
    }
    * {
      margin: 0; padding: 0;
      box-sizing: border-box;
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Oxygen, Ubuntu, Cantarell, "Open Sans", "Helvetica Neue", sans-serif;
    }
    body {
      background-color: var(--background);
      color: var(--foreground);
      line-height: 1.5;
      display: flex;
      height: 100vh;
      overflow: hidden;
    }
    /* Sidebar */
    .sidebar {
      width: 300px;
      background-color: var(--sidebar-bg);
      border-right: 1px solid var(--sidebar-border);
      display: flex;
      flex-direction: column;
      padding: 1rem;
      overflow-y: auto;
      flex-shrink: 0;
    }
    .logo {
      display: flex;
      align-items: center;
      gap: 0.5rem;
      margin-bottom: 1rem;
      font-weight: 700;
      font-size: 1.25rem;
    }
    .logo svg { color: var(--primary); }
    .storage-card {
      background-color: var(--muted);
      border-radius: var(--radius);
      padding: 1rem;
      margin-bottom: 1rem;
    }
    .storage-card h4 {
      font-size: 0.875rem;
      margin-bottom: 0.5rem;
      color: var(--foreground);
      font-weight: 600;
    }
    .storage-usage {
      display: flex;
      align-items: center;
      justify-content: space-between;
      margin-bottom: 0.5rem;
      font-size: 0.875rem;
    }
    .storage-bar {
      height: 6px;
      background-color: var(--border);
      border-radius: 9999px;
      overflow: hidden;
      margin-top: 0.25rem;
    }
    .storage-bar-fill {
      height: 100%;
      background-color: var(--primary);
      width: 0%;
      transition: width 0.3s ease;
    }
    .new-folder {
      display: flex;
      gap: 0.5rem;
      margin-bottom: 1rem;
    }
    .new-folder input {
      flex: 1;
      padding: 0.25rem 0.5rem;
      border: 1px solid var(--border);
      border-radius: var(--radius);
      font-size: 0.875rem;
    }
    .new-folder button {
      border: none;
      border-radius: var(--radius);
      background-color: var(--primary);
      color: var(--primary-foreground);
      padding: 0.5rem 0.75rem;
      cursor: pointer;
      font-size: 0.875rem;
      white-space: nowrap;
    }
    .new-folder button:hover {
      background-color: var(--primary-hover);
    }
    .folders {
      display: flex;
      flex-direction: column;
      gap: 0.25rem;
    }
    .folder-item {
      display: flex;
      align-items: center;
      justify-content: space-between;
      padding: 0.5rem;
      border-radius: var(--radius);
      cursor: pointer;
      transition: background-color 0.2s;
      color: var(--muted-foreground);
    }
    .folder-item:hover {
      background-color: var(--muted);
      color: var(--foreground);
    }
    .folder-left {
      display: flex;
      align-items: center;
      gap: 0.5rem;
    }
    .folder-right {
      display: flex;
      align-items: center;
      gap: 0.5rem;
    }
    .folder-item svg {
      width: 16px;
      height: 16px;
    }
    .folder-trash:hover {
      color: red;
    }
    /* Main content */
    .main {
      flex: 1;
      display: flex;
      flex-direction: column;
      overflow-y: auto;
    }
    .header {
      padding: 1rem;
      border-bottom: 1px solid var(--border);
      display: flex;
      align-items: center;
      justify-content: space-between;
    }
    .header h2 {
      font-size: 1.25rem;
      font-weight: 600;
      margin-right: 1rem;
    }
    .back-btn {
      padding: 0.25rem 0.5rem;
      border: 1px solid var(--border);
      background: var(--muted);
      border-radius: var(--radius);
      cursor: pointer;
      font-size: 0.75rem;
    }
    .upload-container {
      display: flex;
      flex-direction: column;
      align-items: center;
      gap: 1rem;
      padding: 1rem;
      border-bottom: 1px solid var(--border);
    }
    .upload-area {
      border: 2px dashed var(--border);
      border-radius: var(--radius);
      padding: 2rem;
      text-align: center;
      transition: all 0.2s ease;
      cursor: pointer;
      width: 100%;
      max-width: 400px;
    }
    .upload-area:hover {
      border-color: var(--primary);
      background-color: rgba(59,130,246,0.05);
    }
    .upload-area p {
      color: var(--muted-foreground);
      margin-top: 0.5rem;
    }
    .upload-area a {
      color: var(--primary);
      text-decoration: none;
      font-weight: 500;
    }
    .btn {
      display: inline-flex;
      align-items: center;
      gap: 0.5rem;
      border: none;
      border-radius: var(--radius);
      padding: 0.5rem 1rem;
      font-size: 0.875rem;
      cursor: pointer;
      transition: background-color 0.2s;
    }
    .btn-primary {
      background-color: var(--primary);
      color: var(--primary-foreground);
    }
    .btn-primary:hover {
      background-color: var(--primary-hover);
    }
    .btn-outline {
      background-color: transparent;
      border: 1px solid var(--border);
      color: var(--foreground);
    }
    .btn-outline:hover {
      background-color: var(--muted);
    }
    .files-section {
      padding: 1rem;
    }
    .section-header {
      display: flex;
      align-items: center;
      justify-content: space-between;
      margin-bottom: 1rem;
    }
    .file-list {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
      gap: 1rem;
    }
    .file-card {
      background-color: var(--card-bg);
      border: 1px solid var(--border);
      border-radius: var(--radius);
      padding: 1rem;
      display: flex;
      flex-direction: column;
      gap: 0.5rem;
      transition: box-shadow 0.2s;
    }
    .file-card:hover {
      box-shadow: var(--shadow-hover);
    }
    .file-title {
      font-weight: 600;
      display: flex;
      align-items: center;
      gap: 0.5rem;
      word-break: break-all;
    }
    .file-meta {
      font-size: 0.75rem;
      color: var(--muted-foreground);
    }
    .file-actions {
      margin-top: 0.5rem;
      display: flex;
      gap: 0.5rem;
    }
    .empty-state, .loading {
      text-align: center;
      color: var(--muted-foreground);
      padding: 1rem;
    }
    input[type="file"] {
      display: none;
    }
    /* Responsive Layout for smaller screens */
    @media (max-width: 768px) {
      body { flex-direction: column; }
      .sidebar {
        width: 100%;
        border-right: none;
        border-bottom: 1px solid var(--sidebar-border);
        flex-direction: row;
        overflow-x: auto;
        align-items: flex-start;
      }
      .main { height: auto; }
      .file-list { grid-template-columns: 1fr; }
    }
  </style>
</head>
<body>
  <!-- Sidebar -->
  <aside class="sidebar">
    <div class="logo">
      <svg viewBox="0 0 24 24" width="24" height="24" fill="none"
           stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
        <path d="M22 12H2"/>
        <path d="M5.45 5.11L2 12v6a2 2 0 0 0 2 2h16a2 2 0 0 0 2-2v-6l-3.55-6.89"/>
        <line x1="6" y1="16" x2="6.01" y2="16"/>
        <line x1="10" y1="16" x2="10.01" y2="16"/>
      </svg>
      CloudSync
    </div>

    <!-- Storage card -->
    <div class="storage-card">
      <h4>Storage</h4>
      <div class="storage-usage">
        <span id="usedValue">-- GB</span>
        <span id="totalValue">-- GB</span>
      </div>
      <div class="storage-bar">
        <div class="storage-bar-fill" id="storageBarFill"></div>
      </div>
      <p id="freeValue" style="font-size: 0.75rem; margin-top: 0.5rem;">-- GB available</p>
    </div>

    <!-- New folder creation -->
    <div class="new-folder">
      <input type="text" id="newFolderName" placeholder="New folder name"/>
      <button onclick="createFolder()">Create</button>
    </div>

    <!-- Sidebar folder list -->
    <div id="sidebarFolders" class="folders"></div>
  </aside>

  <!-- Main Content -->
  <main class="main">
    <!-- Header with Back button -->
    <div class="header">
      <h2 id="folderTitle">My Files</h2>
      <button class="back-btn" id="backButton" onclick="goBack()" style="display:none;">Back</button>
    </div>

    <!-- Upload Container (centered) -->
    <div class="upload-container">
      <div class="upload-area" id="uploadArea">
        <svg viewBox="0 0 24 24" width="48" height="48" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
          <path d="M21 15v4a2 2 0 0 1-2 2H5
                   a2 2 0 0 1-2-2v-4"/>
          <polyline points="17 8 12 3 7 8"/>
          <line x1="12" y1="3" x2="12" y2="15"/>
        </svg>
        <p>Drag and drop files here or <a href="#" id="browseFiles">browse</a> to upload</p>
        <input type="file" id="fileInput"/>
      </div>
      <button class="btn btn-primary" id="uploadButton" disabled>
        <svg viewBox="0 0 24 24" width="16" height="16" fill="none"
             stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
          <path d="M21 15v4
                   a2 2 0 0 1-2 2H5
                   a2 2 0 0 1-2-2v-4"/>
          <polyline points="17 8 12 3 7 8"/>
          <line x1="12" y1="3" x2="12" y2="15"/>
        </svg>
        Upload
      </button>
    </div>

    <!-- Files Section -->
    <div class="files-section">
      <div class="section-header">
        <h3 style="font-size: 1.125rem; font-weight:600; margin:0;">Files</h3>
        <button class="btn btn-outline" id="refreshButton">
          <svg viewBox="0 0 24 24" width="16" height="16" fill="none"
               stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
            <path d="M21.5 2v6h-6"/>
            <path d="M2.5 22v-6h6"/>
            <path d="M2 11.5A10 10 0 0 1 20.8 7.2"/>
            <path d="M22 12.5A10 10 0 0 1 3.2 16.7"/>
          </svg>
          Refresh
        </button>
      </div>
      <div id="fileList" class="file-list">
        <div class="loading">Loading files...</div>
      </div>
    </div>
  </main>

  <script>
    // Folders we want to hide by default
    const excludedFolders = [
      "Music","Podcasts","Ringtones","Alarms","Notifications",
      "Pictures","Movies","Download","DCIM","Documents",
      "Audiobooks","Recordings","Android"
    ];

    let currentFolder = ""; // track current folder ("" for root)

    // WebSocket for real-time updates
    const socket = new WebSocket("ws://" + window.location.hostname + ":81");
    socket.onopen = () => console.log("WebSocket connected");
    socket.onmessage = (event) => {
      if(event.data.startsWith("uploaded:") || event.data.startsWith("deleted:")){
        loadFiles();
        loadSidebarFolders();
        loadStorage();
      }
    };

    // DOM references
    const uploadArea     = document.getElementById("uploadArea");
    const browseLink     = document.getElementById("browseFiles");
    const fileInput      = document.getElementById("fileInput");
    const uploadBtn      = document.getElementById("uploadButton");
    const refreshBtn     = document.getElementById("refreshButton");
    const fileListEl     = document.getElementById("fileList");
    const backBtn        = document.getElementById("backButton");
    const folderTitle    = document.getElementById("folderTitle");
    const usedValEl      = document.getElementById("usedValue");
    const totalValEl     = document.getElementById("totalValue");
    const freeValEl      = document.getElementById("freeValue");
    const storageBarFill = document.getElementById("storageBarFill");
    const sidebarFolders = document.getElementById("sidebarFolders");
    const newFolderName  = document.getElementById("newFolderName");

    // Disable the upload button until a file is chosen
    uploadBtn.disabled = true;

    // If user picks a file, enable the Upload button
    fileInput.addEventListener("change", () => {
      uploadBtn.disabled = (fileInput.files.length === 0);
    });

    // Drag & drop
    uploadArea.addEventListener("dragover", (e) => {
      e.preventDefault();
      uploadArea.style.borderColor = "var(--primary)";
      uploadArea.style.backgroundColor = "rgba(59,130,246,0.05)";
    });
    uploadArea.addEventListener("dragleave", () => {
      uploadArea.style.borderColor = "var(--border)";
      uploadArea.style.backgroundColor = "transparent";
    });
    uploadArea.addEventListener("drop", (e) => {
      e.preventDefault();
      uploadArea.style.borderColor = "var(--border)";
      uploadArea.style.backgroundColor = "transparent";
      if(e.dataTransfer.files.length > 0){
        fileInput.files = e.dataTransfer.files;
        uploadBtn.disabled = false;
      }
    });

    // Browse link
    browseLink.addEventListener("click", (e) => {
      e.preventDefault();
      fileInput.click();
    });

    // Upload button
    uploadBtn.addEventListener("click", () => {
      if(fileInput.files.length === 0){
        alert("Please select a file to upload.");
        return;
      }
      uploadFile();
    });

    // Refresh button
    refreshBtn.addEventListener("click", () => {
      loadFiles();
      loadSidebarFolders();
      loadStorage();
    });

    // Create folder, then open it
    function createFolder(){
      const name = newFolderName.value.trim();
      if(!name){
        alert("Please enter a folder name.");
        return;
      }
      fetch("/mkdir?name=" + encodeURIComponent(name))
        .then(res => res.text())
        .then(msg => {
          alert(msg);
          if(msg.includes("successfully")){
            newFolderName.value = "";
            currentFolder = name; // open newly created folder
            loadFiles();
          }
          loadSidebarFolders();
        })
        .catch(err => {
          console.error(err);
          alert("Failed to create folder.");
        });
    }

    // Delete folder
    function deleteFolder(folderName){
      fetch("/rmdir?dir=" + encodeURIComponent(folderName))
        .then(res => res.text())
        .then(msg => {
          alert(msg);
          loadSidebarFolders();
          if(currentFolder === folderName){
            currentFolder = "";
            loadFiles();
          }
          loadStorage();
        })
        .catch(err => {
          console.error(err);
          alert("Failed to delete folder.");
        });
    }

    // Upload file -> now includes "folder" param to upload into currentFolder
    function uploadFile(){
      const file = fileInput.files[0];
      if(!file) return;
      uploadBtn.disabled = true;
      uploadBtn.textContent = "Uploading...";

      const formData = new FormData();
      formData.append("file", file);
      // If we are in a subfolder, pass it along:
      formData.append("folder", currentFolder);

      fetch("/upload", {
        method: "POST",
        body: formData
      })
      .then(res => res.text())
      .then(text => {
        alert(text);
        fileInput.value = "";
        loadFiles();       // refresh file list
        loadStorage();     // refresh storage
      })
      .catch(err => {
        console.error(err);
        alert("Upload failed!");
      })
      .finally(() => {
        uploadBtn.disabled = true;
        uploadBtn.textContent = "Upload";
      });
    }

    // Load the top-level folders in the sidebar
    function loadSidebarFolders(){
      sidebarFolders.innerHTML = "";
      fetch("/list?top=1")
        .then(res => res.json())
        .then(data => {
          if(!data || data.length === 0){
            sidebarFolders.innerHTML = "<div style='color: var(--muted-foreground); font-size: 0.875rem;'>No folders yet</div>";
            return;
          }
          data.forEach(dir => {
            // skip typical Android folders if you want them hidden
            if(excludedFolders.includes(dir)) {
              return; 
            }
            const item = document.createElement("div");
            item.className = "folder-item";
            item.innerHTML = `
              <div class="folder-left">
                <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"
                     stroke-linecap="round" stroke-linejoin="round">
                  <path d="M22 19a2 2 0 0 1-2 2H4
                           a2 2 0 0 1-2-2V5
                           a2 2 0 0 1 2-2h5l2 3h9
                           a2 2 0 0 1 2 2z"/>
                </svg>
                <span>${dir}</span>
              </div>
              <div class="folder-right">
                <svg class="folder-trash" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"
                     stroke-linecap="round" stroke-linejoin="round" title="Delete folder">
                  <polyline points="3 6 5 6 21 6"/>
                  <path d="M19 6v14a2 2 0 0 1-2 2H7
                           a2 2 0 0 1-2-2V6m3 0V4
                           a2 2 0 0 1 2-2h4
                           a2 2 0 0 1 2 2v2"/>
                </svg>
              </div>
            `;
            // Clicking folder name = open folder
            item.querySelector(".folder-left").onclick = () => {
              currentFolder = dir;
              loadFiles();
            };
            // Clicking trash icon = delete folder
            item.querySelector(".folder-trash").onclick = (evt) => {
              evt.stopPropagation();
              if(confirm(`Are you sure you want to delete folder '${dir}' and all contents?`)){
                deleteFolder(dir);
              }
            };
            sidebarFolders.appendChild(item);
          });
        })
        .catch(err => {
          console.error(err);
          sidebarFolders.innerHTML = "<div style='color: red;'>Error loading folders</div>";
        });
    }

    // Load files in main area
    function loadFiles(){
      let listUrl = "/list";
      if(currentFolder !== ""){
        listUrl += "?folder=" + encodeURIComponent(currentFolder);
      }
      fileListEl.innerHTML = "<div class='loading'>Loading files...</div>";
      fetch(listUrl)
        .then(res => res.json())
        .then(data => {
          fileListEl.innerHTML = "";
          folderTitle.textContent = currentFolder === "" ? "My Files" : currentFolder;
          backBtn.style.display = currentFolder === "" ? "none" : "block";
          if(!data || data.length === 0){
            fileListEl.innerHTML = "<div class='empty-state'>No files found</div>";
            return;
          }
          data.forEach(entry => {
            // skip typical Android folders if you want them hidden
            if(excludedFolders.includes(entry.name)){
              return; 
            }
            const card = document.createElement("div");
            card.className = "file-card";
            if(entry.isDir === true || entry.isDir === "true"){
              card.innerHTML = `
                <div class="file-title">
                  <svg viewBox="0 0 24 24" width="16" height="16" fill="none"
                       stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
                    <path d="M22 19a2 2 0 0 1-2 2H4
                             a2 2 0 0 1-2-2V5
                             a2 2 0 0 1 2-2h5l2 3h9
                             a2 2 0 0 1 2 2z"/>
                  </svg>
                  ${entry.name}
                </div>
                <div class="file-meta">Folder</div>
                <div class="file-actions">
                  <button class="btn-outline" onclick="openSubFolder('${entry.name}')">Open</button>
                </div>
              `;
            } else {
              const sizeMB = (entry.size / (1024*1024)).toFixed(2);
              card.innerHTML = `
                <div class="file-title">${entry.name}</div>
                <div class="file-meta">${sizeMB} MB</div>
                <div class="file-actions">
                  <button class="btn-outline" onclick="downloadFile('${entry.name}')">Download</button>
                  <button class="btn-outline" onclick="deleteFile('${entry.name}')">Delete</button>
                </div>
              `;
            }
            fileListEl.appendChild(card);
          });
        })
        .catch(err => {
          console.error(err);
          fileListEl.innerHTML = "<div class='empty-state'>Error loading files</div>";
        });
    }

    // Subfolder open
    function openSubFolder(folderName){
      if(currentFolder === ""){
        currentFolder = folderName;
      } else {
        currentFolder += "/" + folderName;
      }
      loadFiles();
    }

    // Back button
    function goBack(){
      if(currentFolder === "") return;
      let parts = currentFolder.split("/");
      parts.pop();
      currentFolder = parts.join("/");
      loadFiles();
    }

    // Download file
    window.downloadFile = function(filename){
      const fullPath = currentFolder ? (currentFolder + "/" + filename) : filename;
      window.location.href = "/download?file=" + encodeURIComponent(fullPath);
    };

    // Delete file
    window.deleteFile = function(filename){
      if(!confirm("Are you sure you want to delete " + filename + "?")) return;
      let fullPath = currentFolder ? (currentFolder + "/" + filename) : filename;
      fetch("/delete?file=" + encodeURIComponent(fullPath))
        .then(res => res.text())
        .then(msg => {
          alert(msg);
          loadFiles();
          loadSidebarFolders();
          loadStorage();
        })
        .catch(err => {
          console.error(err);
          alert("Failed to delete file.");
        });
    };

    // Storage info
    function loadStorage(){
      fetch("/storage")
        .then(res => res.json())
        .then(data => {
          usedValEl.textContent  = data.usedGB.toFixed(2)  + " GB";
          totalValEl.textContent = data.totalGB.toFixed(2) + " GB";
          freeValEl.textContent  = data.freeGB.toFixed(2)  + " GB available";
          let percent = (data.usedGB / data.totalGB) * 100;
          if(percent > 100) percent = 100;
          storageBarFill.style.width = percent.toFixed(1) + "%";
        })
        .catch(err => console.error(err));
    }

    // On load
    window.onload = () => {
      loadSidebarFolders();  // show top-level folders in the sidebar
      loadFiles();           // load root by default
      loadStorage();         // update storage indicator
    };
  </script>
</body>
</html>
)rawliteral";

// ----------------------------------------------------------------
// HANDLERS

void handleRoot() {
  server.send_P(200, "text/html", index_html);
}

// Summarize total/used/free in JSON (for the entire SD card)
void handleStorageInfo() {
  uint64_t totalBytes = SD.cardSize();
  uint64_t usedBytes  = sumFileSizesRecursive("/");  // sum entire card
  uint64_t freeBytes  = (usedBytes < totalBytes) ? (totalBytes - usedBytes) : 0;

  double totalGB = (double)totalBytes / (1024.0 * 1024.0 * 1024.0);
  double usedGB  = (double)usedBytes  / (1024.0 * 1024.0 * 1024.0);
  double freeGB  = totalGB - usedGB;

  char json[128];
  snprintf(json, sizeof(json),
           "{\"totalGB\":%.2f,\"usedGB\":%.2f,\"freeGB\":%.2f}",
           totalGB, usedGB, freeGB);

  server.send(200, "application/json", json);
}

// Create a new folder
void handleMkdir() {
  if(!server.hasArg("name")) {
    server.send(400, "text/plain", "Missing 'name' parameter");
    return;
  }
  String folderName = server.arg("name");
  if(folderName.startsWith("/")) folderName.remove(0,1);
  String path = "/" + folderName;

  if(SD.exists(path)){
    server.send(400, "text/plain", "Folder already exists");
    return;
  }
  if(SD.mkdir(path)){
    server.send(200, "text/plain", "Folder created successfully!");
  } else {
    server.send(500, "text/plain", "Failed to create folder");
  }
}

// Recursively remove a directory
bool removeDirectoryRecursive(const char * path);
void handleRmdir() {
  if(!server.hasArg("dir")) {
    server.send(400, "text/plain", "Missing 'dir' parameter");
    return;
  }
  String dirName = server.arg("dir");
  if(dirName.startsWith("/")) dirName.remove(0,1);
  String path = "/" + dirName;

  if(!SD.exists(path)) {
    server.send(404, "text/plain", "Folder not found");
    return;
  }
  File f = SD.open(path);
  if(!f.isDirectory()) {
    f.close();
    server.send(400, "text/plain", "Not a folder");
    return;
  }
  f.close();

  if(removeDirectoryRecursive(path.c_str())){
    server.send(200, "text/plain", "Folder deleted successfully!");
    // Notify via WebSocket
    String message = "deleted:" + dirName;
    webSocket.broadcastTXT(message);
  } else {
    server.send(500, "text/plain", "Failed to delete folder");
  }
}

// Simple success response for /upload
void handleUpload() {
  server.send(200, "text/plain", "File uploaded successfully!");
}

// The actual file data arrives here
void handleFileUpload() {
  HTTPUpload &upload = server.upload( );
  if (upload.status == UPLOAD_FILE_START) {
    // Check if the user also sent a "folder" param
    String folder = "/";
    if(server.hasArg("folder")) {
      folder += server.arg("folder"); // e.g. "/subfolder"
    }
    // Clean up slashes
    if(folder.endsWith("/")) folder.remove(folder.length()-1);
    if(folder == "") folder = "/";

    String filename = folder + "/" + upload.filename;
    Serial.print("Upload Start: ");
    Serial.println(filename);

    fsUploadFile = SD.open(filename, FILE_WRITE);
    if (!fsUploadFile) {
      Serial.println("Failed to open file for writing");
    }
  }
  else if (upload.status == UPLOAD_FILE_WRITE) {
    if (fsUploadFile) {
      fsUploadFile.write(upload.buf, upload.currentSize);
    }
  }
  else if (upload.status == UPLOAD_FILE_END) {
    if (fsUploadFile) {
      fsUploadFile.close();
      Serial.print("Upload End: ");
      Serial.println(upload.totalSize);
      // Notify via WebSocket
      String message = "uploaded:" + String(upload.filename);
      webSocket.broadcastTXT(message);
    }
  }
}

// Download file using ?file= param
void handleDownload() {
  if(!server.hasArg("file")) {
    server.send(400, "text/plain", "File parameter missing");
    return;
  }
  String filename = "/" + server.arg("file");
  if(!SD.exists(filename)) {
    server.send(404, "text/plain", "File not found");
    return;
  }
  File downloadFile = SD.open(filename, FILE_READ);
  server.sendHeader("Content-Disposition", "attachment; filename=" + server.arg("file"));
  server.streamFile(downloadFile, "application/octet-stream");
  downloadFile.close();
}

// Delete file (only files, not folders)
void handleDelete() {
  if(!server.hasArg("file")) {
    server.send(400, "text/plain", "File parameter missing");
    return;
  }
  String filename = "/" + server.arg("file");
  if(!SD.exists(filename)) {
    server.send(404, "text/plain", "File not found");
    return;
  }
  File f = SD.open(filename);
  bool isDir = f.isDirectory();
  f.close();
  if(isDir) {
    server.send(400, "text/plain", "Deleting folders is not supported via /delete. Use /rmdir instead.");
    return;
  }
  SD.remove(filename);
  server.send(200, "text/plain", "File deleted successfully!");
  // Notify via WebSocket
  String message = "deleted:" + server.arg("file");
  webSocket.broadcastTXT(message);
}

// List top-level directories or a subfolder
void handleListFiles() {
  // If "top=1", list top-level directories only
  if(server.hasArg("top")) {
    File root = SD.open("/");
    if(!root){
      server.send(404, "application/json", "[]");
      return;
    }
    String folderList = "[";
    bool first = true;
    File file = root.openNextFile();
    while(file){
      if(file.isDirectory()){
        String dirName = String(file.name());
        if(dirName.startsWith("/")) dirName = dirName.substring(1);
        if(!first) folderList += ",";
        folderList += "\"" + dirName + "\"";
        first = false;
      }
      file.close();
      file = root.openNextFile();
    }
    folderList += "]";
    server.send(200, "application/json", folderList);
    return;
  }

  // Otherwise, list contents of a specific folder or root
  String folder = "/";
  if(server.hasArg("folder")) {
    folder = server.arg("folder");
    if(!folder.startsWith("/")) folder = "/" + folder;
  }
  File root = SD.open(folder);
  if(!root) {
    server.send(404, "application/json", "[]");
    return;
  }
  String fileList = "[";
  bool first = true;
  File file = root.openNextFile();
  while(file){
    if(!first) fileList += ",";
    String fullName = String(file.name());
    String shortName = fullName;
    if(shortName.startsWith(folder)) {
      shortName = shortName.substring(folder.length());
    }
    if(shortName.startsWith("/")) {
      shortName = shortName.substring(1);
    }
    fileList += "{";
    fileList += "\"name\":\"" + shortName + "\",";
    fileList += "\"size\":" + String(file.size()) + ",";
    fileList += "\"isDir\":" + String(file.isDirectory() ? "true" : "false");
    fileList += "}";
    first = false;
    file.close();
    file = root.openNextFile();
  }
  fileList += "]";
  server.send(200, "application/json", fileList);
}

// WebSocket event
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_CONNECTED:
      Serial.printf("[WS] Client %u connected.\n", num);
      break;
    case WStype_DISCONNECTED:
      Serial.printf("[WS] Client %u disconnected.\n", num);
      break;
    case WStype_TEXT:
      Serial.printf("[WS] Received from %u: %s\n", num, payload);
      break;
    default:
      break;
  }
}

// Setup & Loop
void setup() {
  Serial.begin(115200);
  WiFi.config(local_IP, gateway, subnet);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected! IP: ");
  Serial.println(WiFi.localIP());

  if(!SD.begin(SD_CS_PIN)) {
    Serial.println("SD Card Mount Failed");
    return;
  }
  Serial.println("SD Card initialized.");

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  server.on("/",          HTTP_GET, handleRoot);
  server.on("/storage",   HTTP_GET, handleStorageInfo);
  server.on("/mkdir",     HTTP_GET, handleMkdir);
  server.on("/rmdir",     HTTP_GET, handleRmdir);
  server.on("/list",      HTTP_GET, handleListFiles);
  server.on("/download",  HTTP_GET, handleDownload);
  server.on("/upload",    HTTP_POST, handleUpload, handleFileUpload);
  server.on("/delete",    HTTP_GET, handleDelete);

  server.begin();
  Serial.println("HTTP Server started.");
}

void loop() {
  server.handleClient();
  webSocket.loop();
}

const authView = document.querySelector("#authView");
const appView = document.querySelector("#appView");
const authForms = {
  login: document.querySelector("#loginForm"),
  register: document.querySelector("#registerForm"),
  reset: document.querySelector("#resetForm"),
};
const state = {
  user: null,
  mode: "all",
  groupId: 0,
  contacts: [],
  groups: [],
  searchTimer: null,
  toastTimer: null,
};

async function api(path, options = {}) {
  const response = await fetch(path, {
    credentials: "same-origin",
    ...options,
    headers: {
      ...(options.body instanceof FormData ? {} : { "Content-Type": "application/json" }),
      ...(options.headers || {}),
    },
  });
  const type = response.headers.get("content-type") || "";
  if (!response.ok) {
    const data = type.includes("application/json") ? await response.json() : null;
    throw new Error(data?.error || `Yêu cầu thất bại (${response.status}).`);
  }
  return type.includes("application/json") ? response.json() : response;
}

function showToast(message, isError = false) {
  const toast = document.querySelector("#toast");
  toast.textContent = message;
  toast.classList.toggle("error", isError);
  toast.classList.remove("hidden");
  clearTimeout(state.toastTimer);
  state.toastTimer = setTimeout(() => toast.classList.add("hidden"), 3500);
}

function setAuthMode(mode) {
  for (const [name, form] of Object.entries(authForms))
    form.classList.toggle("hidden", name !== mode);
  document.querySelector("#authTitle").textContent =
    mode === "register" ? "Tạo tài khoản" : mode === "reset" ? "Đặt lại mật khẩu" : "Chào mừng trở lại!";
  document.querySelector("#authSubtitle").textContent =
    mode === "register" ? "Đăng ký để bắt đầu quản lý danh bạ." :
    mode === "reset" ? "Đặt lại mật khẩu bằng mã khôi phục." :
    "Đăng nhập để quản lý danh bạ của bạn.";
}

function showApp(user) {
  state.user = user;
  authView.classList.add("hidden");
  appView.classList.remove("hidden");
  document.querySelector("#userName").textContent = user.name;
  document.querySelector("#adminNav").classList.toggle("hidden", !user.admin);
  loadWorkspace().catch((error) => showToast(error.message, true));
}

async function loadWorkspace() {
  await loadGroups();
  await loadContacts();
  if (state.user?.admin) await loadAdminUsers();
}

async function loadGroups() {
  state.groups = await api("/api/groups");
  renderGroups();
}

function renderGroups() {
  const list = document.querySelector("#groupList");
  list.replaceChildren();
  for (const group of state.groups) {
    const row = document.createElement("div");
    row.className = "group-item";
    const button = document.createElement("button");
    button.className = `nav-button${state.mode === "group" && state.groupId === group.id ? " active" : ""}`;
    button.textContent = group.name;
    button.addEventListener("click", () => {
      state.mode = "group";
      state.groupId = group.id;
      setActiveNav();
      renderGroups();
      document.querySelector("#pageTitle").textContent = group.name;
      loadContacts().catch((error) => showToast(error.message, true));
    });
    const remove = document.createElement("button");
    remove.className = "group-delete";
    remove.type = "button";
    remove.textContent = "×";
    remove.title = "Xóa nhóm";
    remove.addEventListener("click", async () => {
      if (!confirm(`Xóa nhóm "${group.name}"?`)) return;
      try {
        await api(`/api/groups/${group.id}`, { method: "DELETE" });
        if (state.groupId === group.id) {
          state.groupId = 0;
          state.mode = "all";
        }
        await loadGroups();
        await loadContacts();
        showToast("Đã xóa nhóm.");
      } catch (error) { showToast(error.message, true); }
    });
    row.append(button, remove);
    list.append(row);
  }
}

function setActiveNav() {
  document.querySelectorAll(".nav-button[data-mode]").forEach((button) => {
    button.classList.toggle("active", button.dataset.mode === state.mode);
  });
}

async function loadContacts() {
  const params = new URLSearchParams();
  const keyword = document.querySelector("#searchInput").value.trim();
  if (keyword) params.set("q", keyword);
  if (state.mode === "group") params.set("groupId", String(state.groupId));
  if (state.mode === "favorites") params.set("favorites", "true");
  params.set("sort", document.querySelector("#sortSelect").value);
  state.contacts = await api(`/api/contacts?${params}`);
  renderContacts();
}

function renderContacts() {
  const rows = document.querySelector("#contactRows");
  rows.replaceChildren();
  for (const contact of state.contacts) {
    const row = document.createElement("tr");
    const name = document.createElement("td");
    name.textContent = `${contact.lastName} ${contact.firstName}`.trim();
    const phone = document.createElement("td");
    phone.textContent = contact.phone;
    const email = document.createElement("td");
    email.textContent = contact.email || "—";
    const company = document.createElement("td");
    company.textContent = contact.company || "—";
    const groups = document.createElement("td");
    const assigned = state.groups.filter((group) => contact.groupIds.includes(group.id));
    if (assigned.length) {
      for (const group of assigned) {
        const chip = document.createElement("span");
        chip.className = "group-chip";
        chip.textContent = group.name;
        groups.append(chip, " ");
      }
    } else groups.textContent = "—";
    const actions = document.createElement("td");
    const favorite = document.createElement("button");
    favorite.className = "favorite-button";
    favorite.type = "button";
    favorite.title = contact.favorite ? "Bỏ yêu thích" : "Đánh dấu yêu thích";
    favorite.textContent = contact.favorite ? "★" : "☆";
    favorite.addEventListener("click", async () => {
      try { await api(`/api/contacts/${contact.id}/favorite`, { method: "POST" }); await loadContacts(); }
      catch (error) { showToast(error.message, true); }
    });
    const edit = document.createElement("button");
    edit.className = "table-action";
    edit.type = "button";
    edit.textContent = "Sửa";
    edit.addEventListener("click", () => openContactDialog(contact));
    const remove = document.createElement("button");
    remove.className = "table-action danger";
    remove.type = "button";
    remove.textContent = "Xóa";
    remove.addEventListener("click", async () => {
      if (!confirm(`Xóa liên hệ "${contact.lastName} ${contact.firstName}"?`)) return;
      try { await api(`/api/contacts/${contact.id}`, { method: "DELETE" }); await loadContacts(); showToast("Đã xóa liên hệ."); }
      catch (error) { showToast(error.message, true); }
    });
    const assignGroup = document.createElement("select");
    assignGroup.setAttribute("aria-label", `Nhóm của ${contact.fullName}`);
    const noGroup = document.createElement("option");
    noGroup.value = "0";
    noGroup.textContent = "Không nhóm";
    assignGroup.append(noGroup);
    for (const group of state.groups) {
      const option = document.createElement("option");
      option.value = String(group.id);
      option.textContent = group.name;
      assignGroup.append(option);
    }
    assignGroup.value = String(contact.groupIds[0] || 0);
    assignGroup.addEventListener("change", async () => {
      try {
        await api(`/api/contacts/${contact.id}/group`, {
          method: "PUT", body: JSON.stringify({ groupId: Number(assignGroup.value) }),
        });
        await loadContacts();
        showToast("Đã cập nhật nhóm.");
      } catch (error) { showToast(error.message, true); }
    });
    actions.append(favorite, edit, remove);
    row.append(name, phone, email, company, groups, actions);
    rows.append(row);
    if (state.groups.length) {
      const groupRow = document.createElement("tr");
      const cell = document.createElement("td");
      cell.colSpan = 6;
      cell.className = "muted";
      cell.append("Chuyển nhóm: ", assignGroup);
      groupRow.append(cell);
      rows.append(groupRow);
    }
  }
  document.querySelector("#contactCount").textContent = `${state.contacts.length} liên hệ`;
  document.querySelector("#emptyState").classList.toggle("hidden", state.contacts.length !== 0);
}

function openContactDialog(contact = null) {
  const form = document.querySelector("#contactForm");
  form.reset();
  form.elements.id.value = contact?.id || "";
  form.elements.lastName.value = contact?.lastName || "";
  form.elements.firstName.value = contact?.firstName || "";
  form.elements.phone.value = contact?.phone || "";
  form.elements.email.value = contact?.email || "";
  form.elements.company.value = contact?.company || "";
  form.elements.address.value = contact?.address || "";
  form.elements.note.value = contact?.note || "";
  document.querySelector("#contactDialogTitle").textContent =
    contact ? "Sửa liên hệ" : "Thêm liên hệ";
  document.querySelector("#contactDialog").showModal();
}

async function loadAdminUsers() {
  if (!state.user?.admin) return;
  const query = document.querySelector("#adminSearch").value.trim();
  const result = await api(`/api/admin/users?q=${encodeURIComponent(query)}`);
  document.querySelector("#adminCount").textContent = `${result.count} tài khoản`;
  const list = document.querySelector("#adminUsers");
  list.replaceChildren();
  for (const account of result.users) {
    const row = document.createElement("div");
    row.className = "admin-row";
    const info = document.createElement("div");
    const name = document.createElement("strong");
    name.textContent = account.name;
    const status = document.createElement("span");
    status.className = `status${account.active ? "" : " locked"}`;
    status.textContent = account.active ? "Đang hoạt động" : "Đã khóa";
    info.append(name, status);
    const actions = document.createElement("div");
    actions.className = "admin-actions";
    const toggle = document.createElement("button");
    toggle.className = "secondary";
    toggle.textContent = account.active ? "Khóa" : "Mở khóa";
    toggle.addEventListener("click", async () => {
      try {
        await api(`/api/admin/users/${account.id}/active`, {
          method: "PUT", body: JSON.stringify({ active: !account.active }),
        });
        await loadAdminUsers();
        showToast("Đã cập nhật tài khoản.");
      } catch (error) { showToast(error.message, true); }
    });
    const reset = document.createElement("button");
    reset.className = "secondary";
    reset.textContent = "Cấp mã";
    reset.addEventListener("click", async () => {
      try {
        const result = await api(`/api/admin/users/${account.id}/reset-token`, { method: "POST" });
        prompt("Mã đặt lại chỉ dùng một lần, hết hạn sau 15 phút. Gửi riêng cho người dùng:", result.token);
      } catch (error) { showToast(error.message, true); }
    });
    actions.append(toggle, reset);
    row.append(info, actions);
    list.append(row);
  }
}

document.querySelectorAll("[data-view]").forEach((button) =>
  button.addEventListener("click", () => setAuthMode(button.dataset.view)));

document.querySelector("#loginForm").addEventListener("submit", async (event) => {
  event.preventDefault();
  const form = new FormData(event.currentTarget);
  try {
    const user = await api("/api/login", {
      method: "POST",
      body: JSON.stringify({ username: form.get("username"), password: form.get("password") }),
    });
    showApp(user);
  } catch (error) { showToast(error.message, true); }
});

document.querySelector("#registerForm").addEventListener("submit", async (event) => {
  event.preventDefault();
  const form = new FormData(event.currentTarget);
  if (form.get("password") !== form.get("confirmation")) {
    showToast("Hai mật khẩu không khớp.", true);
    return;
  }
  try {
    await api("/api/register", {
      method: "POST",
      body: JSON.stringify({
        fullName: form.get("fullName"), username: form.get("username"),
        email: form.get("email"), password: form.get("password"),
      }),
    });
    event.currentTarget.reset();
    setAuthMode("login");
    document.querySelector("#loginForm").elements.username.value = form.get("username");
    showToast("Tạo tài khoản thành công. Hãy đăng nhập.");
  } catch (error) { showToast(error.message, true); }
});

document.querySelector("#resetForm").addEventListener("submit", async (event) => {
  event.preventDefault();
  const form = new FormData(event.currentTarget);
  if (form.get("password") !== form.get("confirmation")) {
    showToast("Hai mật khẩu không khớp.", true);
    return;
  }
  try {
    await api("/api/password/reset", {
      method: "POST",
      body: JSON.stringify({ token: form.get("token"), password: form.get("password") }),
    });
    event.currentTarget.reset();
    setAuthMode("login");
    showToast("Đặt lại mật khẩu thành công.");
  } catch (error) { showToast(error.message, true); }
});

document.querySelector("#logoutButton").addEventListener("click", async () => {
  try { await api("/api/logout", { method: "POST" }); } finally {
    state.user = null;
    appView.classList.add("hidden");
    authView.classList.remove("hidden");
    setAuthMode("login");
  }
});

document.querySelectorAll(".nav-button[data-mode]").forEach((button) => {
  button.addEventListener("click", () => {
    state.mode = button.dataset.mode;
    state.groupId = 0;
    setActiveNav();
    const adminMode = state.mode === "admin";
    document.querySelector("#contactsPanel").classList.toggle("hidden", adminMode);
    document.querySelector("#adminPanel").classList.toggle("hidden", !adminMode);
    if (adminMode) loadAdminUsers().catch((error) => showToast(error.message, true));
    else {
      document.querySelector("#pageTitle").textContent =
        state.mode === "favorites" ? "Liên hệ yêu thích" : "Tất cả liên hệ";
      loadContacts().catch((error) => showToast(error.message, true));
    }
  });
});

document.querySelector("#searchInput").addEventListener("input", () => {
  clearTimeout(state.searchTimer);
  state.searchTimer = setTimeout(() => loadContacts().catch((error) => showToast(error.message, true)), 200);
});
document.querySelector("#sortSelect").addEventListener("change", () =>
  loadContacts().catch((error) => showToast(error.message, true)));
document.querySelector("#adminSearch").addEventListener("input", () => {
  clearTimeout(state.searchTimer);
  state.searchTimer = setTimeout(() => loadAdminUsers().catch((error) => showToast(error.message, true)), 200);
});
document.querySelector("#addContactButton").addEventListener("click", () => openContactDialog());
document.querySelector("#contactForm").addEventListener("submit", async (event) => {
  event.preventDefault();
  const form = new FormData(event.currentTarget);
  const body = Object.fromEntries(["lastName", "firstName", "phone", "email", "company", "address", "note"]
    .map((key) => [key, String(form.get(key) || "")]));
  const id = form.get("id");
  try {
    await api(id ? `/api/contacts/${id}` : "/api/contacts", {
      method: id ? "PUT" : "POST", body: JSON.stringify(body),
    });
    document.querySelector("#contactDialog").close();
    await loadContacts();
    showToast(id ? "Đã cập nhật liên hệ." : "Đã thêm liên hệ.");
  } catch (error) { showToast(error.message, true); }
});
document.querySelector("#addGroupButton").addEventListener("click", () => document.querySelector("#groupDialog").showModal());
document.querySelector("#groupForm").addEventListener("submit", async (event) => {
  event.preventDefault();
  const name = new FormData(event.currentTarget).get("name");
  try {
    await api("/api/groups", { method: "POST", body: JSON.stringify({ name }) });
    event.currentTarget.reset();
    document.querySelector("#groupDialog").close();
    await loadGroups();
    showToast("Đã tạo nhóm.");
  } catch (error) { showToast(error.message, true); }
});
document.querySelectorAll("[data-close]").forEach((button) =>
  button.addEventListener("click", () => document.getElementById(button.dataset.close).close()));
document.querySelector("#importButton").addEventListener("click", () => document.querySelector("#csvInput").click());
document.querySelector("#csvInput").addEventListener("change", async (event) => {
  const file = event.currentTarget.files[0];
  if (!file) return;
  const body = new FormData();
  body.append("file", file);
  try {
    const result = await api("/api/contacts/import", { method: "POST", body });
    await loadContacts();
    showToast(`Đã nhập ${result.imported}, bỏ qua ${result.skipped} liên hệ.`);
    if (result.errors.length) alert(result.errors.slice(0, 5).join("\n"));
  } catch (error) { showToast(error.message, true); }
  finally { event.currentTarget.value = ""; }
});
document.querySelector("#exportButton").addEventListener("click", async () => {
  try {
    const response = await api("/api/contacts/export");
    const url = URL.createObjectURL(await response.blob());
    const link = document.createElement("a");
    link.href = url;
    link.download = "phonebook.csv";
    link.click();
    URL.revokeObjectURL(url);
  } catch (error) { showToast(error.message, true); }
});

api("/api/session").then(showApp).catch(() => {
  authView.classList.remove("hidden");
  appView.classList.add("hidden");
  setAuthMode("login");
});

const tableBody = document.getElementById('studentTableBody');
const pageInfo = document.getElementById('pageInfo');
const prevPageBtn = document.getElementById('prevPageBtn');
const nextPageBtn = document.getElementById('nextPageBtn');
const searchBtn = document.getElementById('searchBtn');
const resetBtn = document.getElementById('resetBtn');
const addStudentBtn = document.getElementById('addStudentBtn');
const logoutBtn = document.getElementById('logoutBtn');
const modal = document.getElementById('studentModal');
const modalTitle = document.getElementById('modalTitle');
const closeModalBtn = document.getElementById('closeModalBtn');
const cancelModalBtn = document.getElementById('cancelModalBtn');
const studentForm = document.getElementById('studentForm');

let currentPage = 1;
const pageSize = 10;
let total = 0;
let currentList = [];
let isEditMode = false;
let editingOldId = '';

function formEncode(data) {
  return new URLSearchParams(data).toString();
}

function escapeHtml(value) {
  return String(value ?? '')
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;')
    .replace(/'/g, '&#39;');
}

function getField(item, ...keys) {
  for (const key of keys) {
    if (item[key] !== undefined && item[key] !== null) return item[key];
  }
  return '';
}

function formatDate(value) {
  if (!value) return '-';
  return String(value);
}

function normalizeListPayload(data) {
  return Array.isArray(data?.data?.list) ? data.data.list : [];
}

function normalizeTotal(data, list) {
  return Number(data?.data?.total ?? list.length ?? 0) || 0;
}

function buildQueryString() {
  const params = new URLSearchParams();
  params.set('page', String(currentPage));
  params.set('size', String(pageSize));

  const studentNo = document.getElementById('searchStudentNo').value.trim();
  const name = document.getElementById('searchName').value.trim();
  const className = document.getElementById('searchClass').value.trim();

  if (studentNo) params.set('id', studentNo);
  if (name) params.set('name', name);
  if (className) params.set('class_name', className);

  return params.toString();
}

function renderTable(list) {
  currentList = Array.isArray(list) ? list : [];

  if (!currentList.length) {
    tableBody.innerHTML = '<tr><td colspan="8" class="empty-row">暂无数据</td></tr>';
    return;
  }

  tableBody.innerHTML = currentList.map(item => {
    const studentId = getField(item, 'id', 'student_no', 'studentId');
    const safeData = encodeURIComponent(JSON.stringify(item));
    return `
      <tr>
        <td>${escapeHtml(studentId)}</td>
        <td>${escapeHtml(getField(item, 'name'))}</td>
        <td>${escapeHtml(getField(item, 'class_name', 'class'))}</td>
        <td>${escapeHtml(getField(item, 'age'))}</td>
        <td>${escapeHtml(getField(item, 'gender'))}</td>
        <td>${escapeHtml(formatDate(getField(item, 'created_time', 'createdTime')))}</td>
        <td>${escapeHtml(formatDate(getField(item, 'updated_time', 'updatedTime')))}</td>
        <td>
          <div class="action-group">
            <button class="ghost-btn small-btn" type="button" onclick="openEditModal('${safeData}')">编辑</button>
            <button class="ghost-btn small-btn" type="button" onclick="openScorePage('${encodeURIComponent(studentId)}','${encodeURIComponent(getField(item, 'name'))}','${encodeURIComponent(getField(item, 'class_name', 'class'))}')">成绩</button>
            <button class="danger-btn small-btn" type="button" onclick="deleteStudent('${escapeHtml(studentId)}')">删除</button>
          </div>
        </td>
      </tr>
    `;
  }).join('');
}

function updatePagination() {
  const totalPages = Math.max(1, Math.ceil(total / pageSize));
  pageInfo.textContent = `第 ${currentPage} 页 / 共 ${totalPages} 页`;
  prevPageBtn.disabled = currentPage <= 1;
  nextPageBtn.disabled = currentPage >= totalPages;
}

async function safeJson(resp) {
  try {
    return await resp.json();
  } catch {
    return {};
  }
}

async function loadStudents() {
  tableBody.innerHTML = '<tr><td colspan="8" class="empty-row">正在加载数据...</td></tr>';

  try {
    const resp = await fetch(`/api/students?${buildQueryString()}`, {
      method: 'GET',
      credentials: 'include'
    });

    const data = await safeJson(resp);

    if (resp.status === 401) {
      window.location.href = 'login.html';
      return;
    }

    if (!resp.ok || data.code !== 0) {
      throw new Error(data.msg || '获取学生列表失败');
    }

    const list = normalizeListPayload(data);
    total = normalizeTotal(data, list);
    renderTable(list);
    updatePagination();
  } catch (err) {
    tableBody.innerHTML = `<tr><td colspan="8" class="empty-row">${escapeHtml(err.message || '数据加载失败')}</td></tr>`;
    total = 0;
    updatePagination();
  }
}

function openModal(isEdit = false) {
  isEditMode = isEdit;
  modal.classList.remove('hidden');
  modalTitle.textContent = isEdit ? '编辑学生' : '新增学生';
}

function closeModal() {
  modal.classList.add('hidden');
  studentForm.reset();
  document.getElementById('studentId').value = '';
  editingOldId = '';
  isEditMode = false;
}

window.openEditModal = function(encodedItem) {
  try {
    const item = JSON.parse(decodeURIComponent(encodedItem));
    openModal(true);
    const studentId = getField(item, 'id', 'student_no', 'studentId');
    editingOldId = String(studentId);
    document.getElementById('studentId').value = editingOldId;
    document.getElementById('studentNo').value = studentId;
    document.getElementById('studentName').value = getField(item, 'name');
    document.getElementById('studentClass').value = getField(item, 'class_name', 'class');
    document.getElementById('studentAge').value = getField(item, 'age');
    document.getElementById('studentGender').value = getField(item, 'gender');
  } catch (e) {
    alert('读取学生信息失败');
  }
};

window.openScorePage = function(studentId, studentName = '', className = '') {
  if (!studentId) {
    alert('缺少学号');
    return;
  }
  const params = new URLSearchParams();
  params.set('id', studentId);
  if (studentName) params.set('name', studentName);
  if (className) params.set('class_name', className);
  window.location.href = `score.html?${params.toString()}`;
};

window.deleteStudent = async function(studentId) {
  if (!studentId) {
    alert('缺少学号');
    return;
  }
  if (!confirm('确定要删除这条学生记录吗？')) return;

  try {
    const resp = await fetch('/api/students', {
      method: 'DELETE',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body: formEncode({ id: studentId }),
      credentials: 'include'
    });

    const data = await safeJson(resp);

    if (resp.status === 401) {
      window.location.href = 'login.html';
      return;
    }

    if (!resp.ok || data.code !== 0) {
      throw new Error(data.msg || '删除失败');
    }

    alert(data.msg || '删除成功');

    const totalPages = Math.max(1, Math.ceil(Math.max(0, total - 1) / pageSize));
    if (currentPage > totalPages) currentPage = totalPages;

    await loadStudents();
  } catch (err) {
    alert(err.message || '删除失败');
  }
};

studentForm.addEventListener('submit', async (e) => {
  e.preventDefault();

  const studentId = document.getElementById('studentNo').value.trim();
  const name = document.getElementById('studentName').value.trim();
  const className = document.getElementById('studentClass').value.trim();
  const age = document.getElementById('studentAge').value.trim();
  const gender = document.getElementById('studentGender').value.trim();

  if (!studentId || !name || !className || !age || !gender) {
    alert('请填写完整学生信息');
    return;
  }

  const bodyObj = isEditMode
    ? {
        old_id: editingOldId,
        id: studentId,
        name,
        class_name: className,
        age,
        gender
      }
    : {
        id: studentId,
        name,
        class_name: className,
        age,
        gender
      };

  const method = isEditMode ? 'PUT' : 'POST';

  try {
    const resp = await fetch('/api/students', {
      method,
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body: formEncode(bodyObj),
      credentials: 'include'
    });

    const data = await safeJson(resp);

    if (resp.status === 401) {
      window.location.href = 'login.html';
      return;
    }

    if (!resp.ok || data.code !== 0) {
      throw new Error(data.msg || (isEditMode ? '修改失败' : '新增失败'));
    }

    alert(data.msg || (isEditMode ? '修改成功' : '新增成功'));
    closeModal();
    currentPage = 1;
    await loadStudents();
  } catch (err) {
    alert(err.message || (isEditMode ? '修改失败' : '新增失败'));
  }
});

addStudentBtn.addEventListener('click', () => openModal(false));
closeModalBtn.addEventListener('click', closeModal);
cancelModalBtn.addEventListener('click', closeModal);
modal.querySelector('.modal-mask').addEventListener('click', closeModal);

searchBtn.addEventListener('click', async () => {
  currentPage = 1;
  await loadStudents();
});

resetBtn.addEventListener('click', async () => {
  document.getElementById('searchStudentNo').value = '';
  document.getElementById('searchName').value = '';
  document.getElementById('searchClass').value = '';
  currentPage = 1;
  await loadStudents();
});

prevPageBtn.addEventListener('click', async () => {
  if (currentPage > 1) {
    currentPage -= 1;
    await loadStudents();
  }
});

nextPageBtn.addEventListener('click', async () => {
  const totalPages = Math.max(1, Math.ceil(total / pageSize));
  if (currentPage < totalPages) {
    currentPage += 1;
    await loadStudents();
  }
});

logoutBtn.addEventListener('click', () => {
  window.location.href = 'login.html';
});

loadStudents();

const scoreTableBody = document.getElementById('scoreTableBody');
const searchCourseName = document.getElementById('searchCourseName');
const searchCourseBtn = document.getElementById('searchCourseBtn');
const resetCourseBtn = document.getElementById('resetCourseBtn');
const addCourseBtn = document.getElementById('addCourseBtn');
const backBtn = document.getElementById('backBtn');
const studentSummary = document.getElementById('studentSummary');
const scoreSubTitle = document.getElementById('scoreSubTitle');
const studentMetaLine = document.getElementById('studentMetaLine');
const studentClassSummary = document.getElementById('studentClassSummary');

let studentName = '';
let studentClassName = '';

const courseModal = document.getElementById('courseModal');
const courseModalTitle = document.getElementById('courseModalTitle');
const closeCourseModalBtn = document.getElementById('closeCourseModalBtn');
const cancelCourseModalBtn = document.getElementById('cancelCourseModalBtn');
const courseForm = document.getElementById('courseForm');

let studentId = '';
let isEditMode = false;
let editingOldCourseName = '';

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

function safeText(v, fallback = '-') {
  return v === undefined || v === null || v === '' ? fallback : String(v);
}

async function safeJson(resp) {
  try {
    return await resp.json();
  } catch {
    return {};
  }
}

function getStudentIdFromUrl() {
  const params = new URLSearchParams(window.location.search);
  return (params.get('id') || '').trim();
}

function getStudentNameFromUrl() {
  const params = new URLSearchParams(window.location.search);
  return (params.get('name') || '').trim();
}

function getStudentClassFromUrl() {
  const params = new URLSearchParams(window.location.search);
  return (params.get('class_name') || '').trim();
}

function buildScoreQuery() {
  const params = new URLSearchParams();
  params.set('id', studentId);
  const courseName = searchCourseName.value.trim();
  if (courseName) params.set('course_name', courseName);
  return params.toString();
}

function renderScoreTable(courses) {
  const list = Array.isArray(courses) ? courses : [];
  if (!list.length) {
    scoreTableBody.innerHTML = '<tr><td colspan="3" class="empty-row">暂无成绩数据</td></tr>';
    return;
  }

  scoreTableBody.innerHTML = list.map(item => {
    const courseName = safeText(item.course_name, '');
    const courseScore = safeText(item.course_score, '0');
    const encoded = encodeURIComponent(JSON.stringify({ course_name: courseName, course_score: courseScore }));
    return `
      <tr>
        <td>${escapeHtml(courseName)}</td>
        <td>${escapeHtml(courseScore)}</td>
        <td>
          <div class="action-group">
            <button class="ghost-btn small-btn" type="button" onclick="openCourseEditModal('${encoded}')">编辑</button>
            <button class="danger-btn small-btn" type="button" onclick="deleteCourse('${encodeURIComponent(courseName)}')">删除</button>
          </div>
        </td>
      </tr>
    `;
  }).join('');
}

async function loadScores() {
  scoreTableBody.innerHTML = '<tr><td colspan="3" class="empty-row">正在加载数据...</td></tr>';

  try {
    const resp = await fetch(`/api/students/score?${buildScoreQuery()}`, {
      method: 'GET',
      credentials: 'include'
    });

    const data = await safeJson(resp);

    if (resp.status === 401) {
      window.location.href = 'login.html';
      return;
    }

    if (!resp.ok || data.code !== 0) {
      throw new Error(data.msg || '获取成绩失败');
    }

    const courses = Array.isArray(data?.data?.courses) ? data.data.courses : [];
    renderScoreTable(courses);
  } catch (err) {
    scoreTableBody.innerHTML = `<tr><td colspan="3" class="empty-row">${escapeHtml(err.message || '加载失败')}</td></tr>`;
  }
}

function openCourseModal(edit = false) {
  isEditMode = edit;
  courseModal.classList.remove('hidden');
  courseModalTitle.textContent = edit ? '编辑成绩' : '新增成绩';
}

function closeCourseModal() {
  courseModal.classList.add('hidden');
  courseForm.reset();
  isEditMode = false;
  editingOldCourseName = '';
}

window.openCourseEditModal = function(encoded) {
  try {
    const item = JSON.parse(decodeURIComponent(encoded));
    openCourseModal(true);
    editingOldCourseName = String(item.course_name || '');
    document.getElementById('courseName').value = safeText(item.course_name, '');
    document.getElementById('courseScore').value = safeText(item.course_score, '');
  } catch (e) {
    alert('读取课程信息失败');
  }
};

window.deleteCourse = async function(encodedCourseName) {
  const courseName = decodeURIComponent(encodedCourseName || '');
  if (!courseName) {
    alert('缺少课程名称');
    return;
  }
  if (!confirm(`确定要删除课程“${courseName}”吗？`)) return;

  try {
    const resp = await fetch('/api/students/score', {
      method: 'DELETE',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body: formEncode({ id: studentId, course_name: courseName }),
      credentials: 'include'
    });

    const data = await safeJson(resp);

    if (resp.status === 401) {
      window.location.href = 'login.html';
      return;
    }

    if (!resp.ok || data.code !== 0) {
      throw new Error(data.msg || '删除成绩失败');
    }

    alert(data.msg || '删除成功');
    await loadScores();
  } catch (err) {
    alert(err.message || '删除成绩失败');
  }
};

courseForm.addEventListener('submit', async (e) => {
  e.preventDefault();

  const courseName = document.getElementById('courseName').value.trim();
  const courseScore = document.getElementById('courseScore').value.trim();

  if (!studentId) {
    alert('缺少学号');
    return;
  }
  if (!courseName || courseScore === '') {
    alert('请填写完整课程信息');
    return;
  }

  const bodyObj = isEditMode
    ? {
        id: studentId,
        course_name: courseName,
        course_score: courseScore,
        old_course_name: editingOldCourseName
      }
    : {
        id: studentId,
        course_name: courseName,
        course_score: courseScore
      };

  const method = isEditMode ? 'PUT' : 'POST';

  try {
    const resp = await fetch('/api/students/score', {
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
      throw new Error(data.msg || (isEditMode ? '更新成绩失败' : '添加成绩失败'));
    }

    alert(data.msg || (isEditMode ? '更新成功' : '添加成功'));
    closeCourseModal();
    await loadScores();
  } catch (err) {
    alert(err.message || (isEditMode ? '更新成绩失败' : '添加成绩失败'));
  }
});

searchCourseBtn.addEventListener('click', loadScores);
resetCourseBtn.addEventListener('click', async () => {
  searchCourseName.value = '';
  await loadScores();
});
addCourseBtn.addEventListener('click', () => openCourseModal(false));
closeCourseModalBtn.addEventListener('click', closeCourseModal);
cancelCourseModalBtn.addEventListener('click', closeCourseModal);
courseModal.querySelector('.modal-mask').addEventListener('click', closeCourseModal);
backBtn.addEventListener('click', () => {
  window.location.href = 'index.html';
});

function initPage() {
  studentId = getStudentIdFromUrl();
  studentName = decodeURIComponent(getStudentNameFromUrl());
  studentClassName = decodeURIComponent(getStudentClassFromUrl());

  if (!studentId) {
    alert('缺少学生学号');
    window.location.href = 'index.html';
    return;
  }

  studentSummary.textContent = studentName ? `当前学生：${studentName}` : `当前学生学号：${studentId}`;
  studentClassSummary.textContent = `班级：${studentClassName || '-'}`;
  scoreSubTitle.textContent = `当前学号：${studentId}`;
  studentMetaLine.textContent = `姓名：${studentName || '-'}　　班级：${studentClassName || '-'}`;
  loadScores();
}

initPage();

const tabs = document.querySelectorAll('.tab-btn');
const panels = document.querySelectorAll('.tab-panel');
const authMessage = document.getElementById('authMessage');
const loginForm = document.getElementById('loginForm');
const registerForm = document.getElementById('registerForm');

function showMessage(message, isError = true) {
  authMessage.textContent = message;
  authMessage.style.color = isError ? '#dc2626' : '#16a34a';
}

function switchTab(tabName) {
  tabs.forEach(btn => btn.classList.toggle('active', btn.dataset.tab === tabName));
  panels.forEach(panel => panel.classList.toggle('active', panel.id === `${tabName}Form`));
  showMessage('', true);
}

function formEncode(data) {
  return new URLSearchParams(data).toString();
}

tabs.forEach(btn => {
  btn.addEventListener('click', () => switchTab(btn.dataset.tab));
});

loginForm.addEventListener('submit', async (e) => {
  e.preventDefault();
  showMessage('');

  const username = document.getElementById('loginUsername').value.trim();
  const password = document.getElementById('loginPassword').value;

  if (!username || !password) {
    showMessage('请输入用户名和密码');
    return;
  }

  try {
    const response = await fetch('/api/login', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/x-www-form-urlencoded'
      },
      credentials: 'include',
      body: formEncode({ username, password })
    });

    const result = await response.json();

    if (!response.ok || result.code !== 0) {
      showMessage(result.msg || '登录失败');
      return;
    }

    showMessage('登录成功，正在跳转...', false);
    setTimeout(() => {
      window.location.href = 'index.html';
    }, 500);
  } catch (error) {
    showMessage('网络异常，请稍后重试');
  }
});

registerForm.addEventListener('submit', async (e) => {
  e.preventDefault();
  showMessage('');

  const username = document.getElementById('registerUsername').value.trim();
  const password = document.getElementById('registerPassword').value;
  const confirmPassword = document.getElementById('confirmPassword').value;

  if (!username || !password || !confirmPassword) {
    showMessage('请填写完整注册信息');
    return;
  }

  if (password.length < 6) {
    showMessage('密码长度不能少于 6 位');
    return;
  }

  if (password !== confirmPassword) {
    showMessage('两次输入的密码不一致');
    return;
  }

  try {
    const response = await fetch('/api/register', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/x-www-form-urlencoded'
      },
      credentials: 'include',
      body: formEncode({ username, password })
    });

    const result = await response.json();

    if (!response.ok || result.code !== 0) {
      showMessage(result.msg || '注册失败');
      return;
    }

    showMessage('注册成功，请登录', false);
    registerForm.reset();
    switchTab('login');
  } catch (error) {
    showMessage('网络异常，请稍后重试');
  }
});

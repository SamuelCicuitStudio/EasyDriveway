/* login.js — server-driven redirect (no fetch) */
(() => {
  'use strict';

  const form     = document.getElementById('loginForm');
  const userEl   = document.getElementById('username');
  const passEl   = document.getElementById('password');
  const toggle   = document.getElementById('toggle');
  const remember = document.getElementById('remember');
  const errBox   = document.getElementById('errorBox');

  // Prefill last-used username (optional)
  try {
    const last = localStorage.getItem('icm_last_user');
    if (last) userEl.value = last;
  } catch {}

  // Show/hide password
  toggle.addEventListener('click', () => {
    const t = passEl.getAttribute('type') === 'password' ? 'text' : 'password';
    passEl.setAttribute('type', t);
    toggle.textContent = t === 'password' ? '👁' : '🙈';
  });

  // Do not prevent default; let the browser POST and follow server 302
  form.addEventListener('submit', () => {
    try {
      if (remember && remember.checked) localStorage.setItem('icm_last_user', userEl.value);
      else localStorage.removeItem('icm_last_user');
    } catch {}

    // Tiny “working” hint (will be replaced by the new page on redirect)
    errBox.hidden = false;
    errBox.textContent = 'Signing in…';
  });
})();

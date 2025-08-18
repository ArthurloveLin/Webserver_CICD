// TinyWebServer博客系统主要JavaScript文件

// 页面加载完成后执行
document.addEventListener('DOMContentLoaded', function() {
    initializeComments();
    initializeLikes();
    initializeSearch();
});

// 评论功能
function initializeComments() {
    const commentForm = document.getElementById('comment-form');
    if (commentForm) {
        commentForm.addEventListener('submit', handleCommentSubmit);
    }
}

function handleCommentSubmit(event) {
    event.preventDefault();
    
    const form = event.target;
    const formData = new FormData(form);
    
    // 显示加载状态
    const submitBtn = form.querySelector('button[type="submit"]');
    const originalText = submitBtn.textContent;
    submitBtn.textContent = '提交中...';
    submitBtn.disabled = true;
    
    // 发送评论
    fetch('/blog/api/comments', {
        method: 'POST',
        body: formData
    })
    .then(response => response.json())
    .then(data => {
        if (data.success) {
            // 添加新评论到页面
            addCommentToPage(data.comment);
            // 清空表单
            form.reset();
            showMessage('评论发表成功！', 'success');
        } else {
            showMessage(data.message || '评论发表失败', 'error');
        }
    })
    .catch(error => {
        console.error('Error:', error);
        showMessage('网络错误，请稍后重试', 'error');
    })
    .finally(() => {
        submitBtn.textContent = originalText;
        submitBtn.disabled = false;
    });
}

function addCommentToPage(comment) {
    const commentsContainer = document.querySelector('.comments');
    if (!commentsContainer) return;
    
    const commentElement = document.createElement('div');
    commentElement.className = 'comment';
    commentElement.innerHTML = `
        <div class="comment-author">${escapeHtml(comment.user_name)}</div>
        <div class="comment-time">${comment.created_at}</div>
        <div class="comment-content">${escapeHtml(comment.content)}</div>
    `;
    
    // 插入到评论列表的开头
    const firstComment = commentsContainer.querySelector('.comment');
    if (firstComment) {
        commentsContainer.insertBefore(commentElement, firstComment);
    } else {
        commentsContainer.appendChild(commentElement);
    }
}

// 点赞功能
function initializeLikes() {
    const likeButtons = document.querySelectorAll('.like-btn');
    likeButtons.forEach(button => {
        button.addEventListener('click', handleLikeClick);
    });
}

function handleLikeClick(event) {
    event.preventDefault();
    
    const button = event.target;
    const targetType = button.dataset.type;
    const targetId = button.dataset.id;
    
    if (!targetType || !targetId) {
        console.error('Missing like target information');
        return;
    }
    
    // 防止重复点击
    if (button.disabled) return;
    button.disabled = true;
    
    fetch('/blog/api/like', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        },
        body: JSON.stringify({
            target_type: targetType,
            target_id: parseInt(targetId)
        })
    })
    .then(response => response.json())
    .then(data => {
        if (data.success) {
            // 更新点赞数
            const countElement = button.querySelector('.like-count');
            if (countElement) {
                countElement.textContent = data.like_count;
            }
            
            // 更新按钮状态
            if (data.liked) {
                button.classList.add('liked');
            } else {
                button.classList.remove('liked');
            }
        } else {
            showMessage(data.message || '操作失败', 'error');
        }
    })
    .catch(error => {
        console.error('Error:', error);
        showMessage('网络错误，请稍后重试', 'error');
    })
    .finally(() => {
        button.disabled = false;
    });
}

// 搜索功能
function initializeSearch() {
    const searchForm = document.getElementById('search-form');
    const searchInput = document.getElementById('search-input');
    
    if (searchForm && searchInput) {
        searchForm.addEventListener('submit', handleSearchSubmit);
        
        // 实时搜索建议（可选）
        let searchTimeout;
        searchInput.addEventListener('input', function() {
            clearTimeout(searchTimeout);
            searchTimeout = setTimeout(() => {
                showSearchSuggestions(this.value);
            }, 300);
        });
    }
}

function handleSearchSubmit(event) {
    event.preventDefault();
    
    const form = event.target;
    const keyword = form.querySelector('#search-input').value.trim();
    
    if (!keyword) {
        showMessage('请输入搜索关键词', 'warning');
        return;
    }
    
    // 跳转到搜索结果页面
    window.location.href = `/blog/search?q=${encodeURIComponent(keyword)}`;
}

function showSearchSuggestions(keyword) {
    if (!keyword.trim()) {
        hideSearchSuggestions();
        return;
    }
    
    // 这里可以实现搜索建议功能
    // fetch('/blog/api/search/suggestions?q=' + encodeURIComponent(keyword))
    // .then(response => response.json())
    // .then(data => {
    //     displaySearchSuggestions(data.suggestions);
    // });
}

function hideSearchSuggestions() {
    const suggestionsContainer = document.getElementById('search-suggestions');
    if (suggestionsContainer) {
        suggestionsContainer.style.display = 'none';
    }
}

// 消息提示功能
function showMessage(message, type = 'info') {
    // 移除现有消息
    const existingMessage = document.querySelector('.message-toast');
    if (existingMessage) {
        existingMessage.remove();
    }
    
    // 创建新消息
    const messageElement = document.createElement('div');
    messageElement.className = `message-toast message-${type}`;
    messageElement.textContent = message;
    
    // 添加样式
    messageElement.style.cssText = `
        position: fixed;
        top: 20px;
        right: 20px;
        padding: 12px 20px;
        border-radius: 4px;
        color: white;
        font-weight: 500;
        z-index: 1000;
        animation: slideIn 0.3s ease;
    `;
    
    // 根据类型设置背景色
    switch (type) {
        case 'success':
            messageElement.style.backgroundColor = '#28a745';
            break;
        case 'error':
            messageElement.style.backgroundColor = '#dc3545';
            break;
        case 'warning':
            messageElement.style.backgroundColor = '#ffc107';
            messageElement.style.color = '#212529';
            break;
        default:
            messageElement.style.backgroundColor = '#007bff';
    }
    
    document.body.appendChild(messageElement);
    
    // 3秒后自动消失
    setTimeout(() => {
        messageElement.style.animation = 'slideOut 0.3s ease';
        setTimeout(() => {
            if (messageElement.parentNode) {
                messageElement.remove();
            }
        }, 300);
    }, 3000);
}

// 工具函数
function escapeHtml(text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}

function formatDate(dateString) {
    const date = new Date(dateString);
    const now = new Date();
    const diffMs = now - date;
    const diffDays = Math.floor(diffMs / (1000 * 60 * 60 * 24));
    
    if (diffDays === 0) {
        return '今天';
    } else if (diffDays === 1) {
        return '昨天';
    } else if (diffDays < 7) {
        return `${diffDays}天前`;
    } else {
        return date.toLocaleDateString('zh-CN');
    }
}

// 添加动画样式
const style = document.createElement('style');
style.textContent = `
    @keyframes slideIn {
        from {
            transform: translateX(100%);
            opacity: 0;
        }
        to {
            transform: translateX(0);
            opacity: 1;
        }
    }
    
    @keyframes slideOut {
        from {
            transform: translateX(0);
            opacity: 1;
        }
        to {
            transform: translateX(100%);
            opacity: 0;
        }
    }
    
    .like-btn {
        background: none;
        border: 1px solid #ddd;
        padding: 0.5rem 1rem;
        border-radius: 4px;
        cursor: pointer;
        transition: all 0.2s ease;
        display: inline-flex;
        align-items: center;
        gap: 0.5rem;
    }
    
    .like-btn:hover {
        border-color: #007bff;
        color: #007bff;
    }
    
    .like-btn.liked {
        background: #007bff;
        color: white;
        border-color: #007bff;
    }
    
    .like-btn:disabled {
        opacity: 0.6;
        cursor: not-allowed;
    }
`;
document.head.appendChild(style);
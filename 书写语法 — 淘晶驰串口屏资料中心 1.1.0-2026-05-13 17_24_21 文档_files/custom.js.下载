document.addEventListener("DOMContentLoaded", function() {
    // 为所有外部链接添加 target="_blank"
    document.querySelectorAll('a[href^="http"]').forEach(link => {
        if (link.hostname !== window.location.hostname) {
            link.setAttribute("target", "_blank");
        }
    });
});

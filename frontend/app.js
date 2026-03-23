document.addEventListener("DOMContentLoaded", () => {
    const terminal = document.getElementById('terminal-log');
    const feedContainer = document.getElementById('feed-container');
    const statusText = document.getElementById('daemon-status');
    const pulse = document.querySelector('.pulse');
    
    let eventSource;
    
    function connectSSE() {
        appendTerminal(`Intentando conectar con el pipeline C++ (wss) ...`);
        
        eventSource = new EventSource('/api/stream');
        
        eventSource.onopen = () => {
            statusText.innerText = "Enlace Neural Activo";
            statusText.style.color = "#10b981";
            pulse.style.backgroundColor = "#10b981";
            appendTerminal(`✅ Flujo de Server-Sent Events abierto con éxito.`);
        };
        
        eventSource.onmessage = (event) => {
            try {
                const data = JSON.parse(event.data);
                appendTerminal(`> ${data.msg}`);
                
                // Si un pipeline extrae el payload formal (Simulación temporal de eventos del C++):
                if (data.type === 'evaluator_hit') {
                    addFeedCard(data.payload);
                }
            } catch(e) {
                appendTerminal(`[Raw Data] ${event.data}`);
            }
        };
        
        eventSource.onerror = () => {
            statusText.innerText = "Desconectado (Reconectando...)";
            statusText.style.color = "#f43f5e";
            pulse.style.backgroundColor = "#f43f5e";
            eventSource.close();
            setTimeout(connectSSE, 5000);
        };
    }
    
    function appendTerminal(text) {
        terminal.innerHTML += `\n[${new Date().toLocaleTimeString('es-CL', { hour12: false })}] ${text}`;
        terminal.scrollTop = terminal.scrollHeight;
    }
    
    function addFeedCard(payload) {
        const placeholder = document.querySelector('.placeholder-text');
        if (placeholder) placeholder.remove();
        
        const card = document.createElement('div');
        const isApproved = payload.aprobado;
        card.className = `feed-item ${isApproved ? 'success' : 'error'}`;
        
        card.innerHTML = `
            <div style="display: flex; justify-content: space-between; align-items: flex-start; margin-bottom: 8px;">
                <div style="font-weight: 700; font-size: 1.1rem; color: #fff;">
                    ${payload.title || 'Documento Extraído'}
                </div>
            </div>
            <div style="font-size: 0.85rem; color: var(--accent-secondary); margin-bottom: 12px; word-break: break-all;">
                <a href="${payload.url}" target="_blank" style="color: inherit; text-decoration: none;">${payload.url || '#'}</a>
            </div>
            <div style="font-size: 0.95rem; line-height: 1.5; color: #cbd5e1; margin-bottom: 15px;">
                ${payload.resumen || (payload.rechazo_motivo ? `RECHAZADO: ${payload.rechazo_motivo}` : 'Sin datos descriptivos.')}
            </div>
            <div style="display: flex; flex-wrap: wrap; gap: 8px; font-size: 0.75rem;">
                <span style="background: rgba(236,72,153,0.15); color: #fbcfe8; padding: 4px 10px; border-radius: 12px; font-weight: 600; border: 1px solid rgba(236,72,153,0.3);">
                    Axioma 0: ${isApproved ? '✅ Cumplido' : '❌ Violado'}
                </span>
                <span style="background: rgba(139,92,246,0.15); color: #ddd6fe; padding: 4px 10px; border-radius: 12px; font-weight: 600; border: 1px solid rgba(139,92,246,0.3);">
                    Rigor: ${payload.rigor ? (payload.rigor * 100).toFixed(0) : 0}%
                </span>
            </div>
        `;
        feedContainer.prepend(card);
    }
    
    // Iniciar SSE
    connectSSE();
    
    // ==========================================
    // Telemetría Falsa (Hasta amarrar GraphQL nativo)
    // ==========================================
    setInterval(() => {
        const nodes = document.getElementById('node-count');
        const current = parseInt(nodes.innerText.replace(/,/g, '')) || 8421;
        nodes.innerText = (current + Math.floor(Math.random() * 2)).toLocaleString('es-CL');
    }, 4500);
    
    setInterval(() => {
        const urls = document.getElementById('url-count');
        const current = parseInt(urls.innerText.replace(/,/g, '')) || 23514;
        urls.innerText = (current + Math.floor(Math.random() * 8)).toLocaleString('es-CL');
    }, 2000);


});

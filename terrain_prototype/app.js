(function () {
  'use strict';

  const MAX_FILE_BYTES = 15 * 1024 * 1024;
  const MAX_DIMENSION = 1024;
  const ACCEPTED_FORMATS = {
    jpg: 'JPEG', jpeg: 'JPEG', png: 'PNG', webp: 'WEBP'
  };

  function validateFile(file) {
    if (!file) return { valid: false, error: 'Choose an image file to continue.' };
    const extension = file.name.split('.').pop().toLowerCase();
    if (!Object.prototype.hasOwnProperty.call(ACCEPTED_FORMATS, extension)) {
      return { valid: false, error: 'Unsupported format. Choose a JPG, JPEG, PNG, or WEBP image.' };
    }
    if (file.size > MAX_FILE_BYTES) {
      return { valid: false, error: 'This file exceeds the 15 MB limit.' };
    }
    if (file.size === 0) return { valid: false, error: 'This file is empty.' };
    return { valid: true, format: ACCEPTED_FORMATS[extension] };
  }

  function fitDimensions(width, height, maximum) {
    if (!Number.isFinite(width) || !Number.isFinite(height) || width <= 0 || height <= 0 || maximum <= 0) {
      throw new Error('Image dimensions must be positive numbers.');
    }
    const scale = Math.min(1, maximum / Math.max(width, height));
    return { width: Math.max(1, Math.round(width * scale)), height: Math.max(1, Math.round(height * scale)) };
  }

  if (typeof module !== 'undefined' && module.exports) {
    module.exports = { validateFile, fitDimensions, MAX_FILE_BYTES, MAX_DIMENSION };
  }

  if (typeof document === 'undefined') return;

  const input = document.getElementById('file-input');
  const dropZone = document.getElementById('drop-zone');
  const message = document.getElementById('message');
  const originalImage = document.getElementById('original-preview');
  const processedImage = document.getElementById('processed-preview');
  const originalMeta = document.getElementById('original-meta');
  const processedMeta = document.getElementById('processed-meta');
  const downloadButton = document.getElementById('download-button');
  const exportDetail = document.getElementById('export-detail');
  let originalUrl = null;
  let processedUrl = null;
  let processedBlob = null;
  let requestId = 0;

  function setMessage(text, kind) {
    message.textContent = text;
    message.className = 'message' + (kind ? ' ' + kind : '');
  }

  function formatBytes(bytes) {
    return bytes >= 1024 * 1024 ? (bytes / (1024 * 1024)).toFixed(2) + ' MB' : (bytes / 1024).toFixed(1) + ' KB';
  }

  function metadata(name, dimensions, details) {
    return '<span class="meta-name" title="' + escapeText(name) + '">' + escapeText(name) + '</span><span class="meta-spec">' + dimensions.width + ' × ' + dimensions.height + ' PX <i>·</i> ' + details + '</span>';
  }

  function escapeText(value) {
    return String(value).replace(/[&<>"']/g, character => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' })[character]);
  }

  function clearOutput() {
    if (originalUrl) URL.revokeObjectURL(originalUrl);
    if (processedUrl) URL.revokeObjectURL(processedUrl);
    originalUrl = processedUrl = null;
    processedBlob = null;
    originalImage.hidden = processedImage.hidden = true;
    originalImage.removeAttribute('src');
    processedImage.removeAttribute('src');
    document.querySelector('#original-stage .empty-state').hidden = false;
    document.querySelector('#processed-stage .empty-state').hidden = false;
    originalMeta.innerHTML = '<span class="empty-label">NO IMAGE LOADED</span>';
    processedMeta.innerHTML = '<span class="empty-label">MAXIMUM DIMENSION 1024 PX</span>';
    downloadButton.disabled = true;
    exportDetail.textContent = 'Processed output will be saved as terrain_input.jpg';
  }

  async function processFile(file) {
    const currentRequest = ++requestId;
    clearOutput();
    const validation = validateFile(file);
    if (!validation.valid) {
      setMessage(validation.error, 'error');
      return;
    }
    setMessage('Loading image for local processing…');
    const sourceUrl = URL.createObjectURL(file);
    originalUrl = sourceUrl;
    const image = new Image();
    image.onload = () => {
      if (currentRequest !== requestId) return;
      try {
        const dimensions = { width: image.naturalWidth, height: image.naturalHeight };
        if (!dimensions.width || !dimensions.height) throw new Error('The selected file does not contain a usable image.');
        originalImage.src = sourceUrl;
        originalImage.hidden = false;
        document.querySelector('#original-stage .empty-state').hidden = true;
        originalMeta.innerHTML = metadata(file.name, dimensions, formatBytes(file.size) + ' <i>·</i> ' + validation.format);

        const outputDimensions = fitDimensions(dimensions.width, dimensions.height, MAX_DIMENSION);
        const canvas = document.createElement('canvas');
        canvas.width = outputDimensions.width;
        canvas.height = outputDimensions.height;
        const context = canvas.getContext('2d');
        if (!context) throw new Error('This browser could not prepare the image canvas.');
        context.fillStyle = '#ffffff';
        context.fillRect(0, 0, canvas.width, canvas.height);
        context.drawImage(image, 0, 0, canvas.width, canvas.height);
        canvas.toBlob(blob => {
          if (currentRequest !== requestId) return;
          if (!blob) {
            setMessage('JPEG export failed. Try another image or browser.', 'error');
            return;
          }
          processedBlob = blob;
          processedUrl = URL.createObjectURL(blob);
          processedImage.src = processedUrl;
          processedImage.hidden = false;
          document.querySelector('#processed-stage .empty-state').hidden = true;
          processedMeta.innerHTML = metadata('terrain_input.jpg', outputDimensions, formatBytes(blob.size) + ' <i>·</i> JPEG');
          downloadButton.disabled = false;
          exportDetail.textContent = dimensions.width + ' × ' + dimensions.height + ' → ' + outputDimensions.width + ' × ' + outputDimensions.height + ' px';
          setMessage('Image processed successfully · ' + outputDimensions.width + ' × ' + outputDimensions.height + ' px', 'success');
          canvas.width = canvas.height = 0;
        }, 'image/jpeg', 0.9);
      } catch (error) {
        setMessage(error.message || 'Could not process this image.', 'error');
      }
    };
    image.onerror = () => {
      if (currentRequest === requestId) setMessage('Could not decode this image. Check that the file is a valid JPG, PNG, or WEBP.', 'error');
      URL.revokeObjectURL(sourceUrl);
      if (originalUrl === sourceUrl) originalUrl = null;
    };
    image.src = sourceUrl;
  }

  input.addEventListener('change', event => {
    if (event.target.files.length) processFile(event.target.files[0]);
    input.value = '';
  });
  dropZone.addEventListener('keydown', event => {
    if (event.key === 'Enter' || event.key === ' ') {
      event.preventDefault();
      input.click();
    }
  });
  ['dragenter', 'dragover'].forEach(type => dropZone.addEventListener(type, event => {
    event.preventDefault();
    dropZone.classList.add('dragging');
  }));
  ['dragleave', 'drop'].forEach(type => dropZone.addEventListener(type, event => {
    event.preventDefault();
    dropZone.classList.remove('dragging');
  }));
  dropZone.addEventListener('drop', event => {
    const file = event.dataTransfer.files[0];
    if (file) processFile(file);
  });
  downloadButton.addEventListener('click', () => {
    if (!processedBlob) return;
    const link = document.createElement('a');
    link.href = processedUrl;
    link.download = 'terrain_input.jpg';
    link.click();
  });
})();

#pragma once
#include <QList>
#include <QPixmap>
#include <QString>
#include <QDir>
#include <QFile>

// ─────────────────────────────────────────────────────────────────────────────
// OcrExtractor
//
// Extracts text from QPixmap screenshots using the Windows built-in OCR
// engine (Windows.Media.Ocr), which ships with every Windows 10/11 install.
//
// Implementation:
//   - Saves each screenshot to a temp PNG in %TEMP%
//   - Launches a hidden PowerShell process to run Windows.Media.Ocr on it
//   - Reads the extracted text from stdout and deletes the temp file
//   - No visible window, no files left on disk, completes in < 500ms
//   - Works with ANY AI model (text + vision), not just vision models
// ─────────────────────────────────────────────────────────────────────────────
class OcrExtractor {
public:
    // Extract and combine text from all screenshots.
    // Returns empty string if OCR fails or no text is found.
    static QString extractText(const QList<QPixmap>& screenshots);

private:
    // OCR a single image and return its text.
    static QString ocrSingle(const QPixmap& pixmap, int index);
};

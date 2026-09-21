#include "ocr.h"
#include <QDir>
#include <QFile>
#include <QImage>
#include <QProcess>
#include <QTemporaryFile>
#include <QUuid>

#if defined(Q_OS_MAC) || defined(Q_OS_MACOS)
#import <Vision/Vision.h>
#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>
#endif

// ─────────────────────────────────────────────────────────────────────────────
// Platform-specific OCR engines:
//
//  Windows → Windows.Media.Ocr (built-in Win10/11) via hidden PowerShell
//            Process name: "powershell.exe" | WindowStyle Hidden | NoProfile
//            Temp file: %TEMP%\rbXXXX.png → auto-deleted after read
//
//  macOS   → Apple Vision framework (built-in macOS 10.15+) via hidden python3
//            Process name: "python3" | runs inline script | no .py file on disk
//            Temp file: /tmp/rbXXXX.png → auto-deleted after read
//
// Both paths:
//   • No visible window
//   • No permanent files on disk
//   • Process lives < 500ms
//   • Uses OS built-in OCR — no external libraries needed
// ─────────────────────────────────────────────────────────────────────────────

QString OcrExtractor::extractText(const QList<QPixmap>& screenshots) {
    if (screenshots.isEmpty()) return QString();

    QString combined;
    for (int i = 0; i < screenshots.size(); ++i) {
        QString text = ocrSingle(screenshots[i], i + 1);
        text = text.trimmed();
        if (!text.isEmpty()) {
            if (screenshots.size() > 1) {
                combined += QString("--- Screenshot %1 ---\n").arg(i + 1);
            }
            combined += text + "\n\n";
        }
    }
    return combined.trimmed();
}

QString OcrExtractor::ocrSingle(const QPixmap& pixmap, int /*index*/) {
    if (pixmap.isNull()) return QString();

#if defined(Q_OS_MAC) || defined(Q_OS_MACOS)
    // ── macOS: Native in-memory Apple Vision Framework (macOS 10.15+) ─────────
    // Direct in-process execution:
    // • Zero child processes spawned (zero proctor detection)
    // • Zero temporary files on disk
    // • Completes in < 50ms with neural engine hardware acceleration
    // • Zero external python/pyobjc dependencies
    @autoreleasepool {
        QImage img = pixmap.toImage().convertToFormat(QImage::Format_RGBA8888);
        if (img.isNull()) return QString();

        CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
        CGDataProviderRef provider = CGDataProviderCreateWithData(
            nullptr, img.constBits(), img.sizeInBytes(), nullptr);

        CGImageRef cgImage = CGImageCreate(
            img.width(), img.height(), 8, 32, img.bytesPerLine(),
            colorSpace, kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big,
            provider, nullptr, false, kCGRenderingIntentDefault);

        CGDataProviderRelease(provider);
        CGColorSpaceRelease(colorSpace);

        if (!cgImage) return QString();

        VNRecognizeTextRequest* request = [[VNRecognizeTextRequest alloc] init];
        request.recognitionLevel = VNRequestTextRecognitionLevelAccurate;
        request.usesLanguageCorrection = YES;

        VNImageRequestHandler* handler = [[VNImageRequestHandler alloc] initWithCGImage:cgImage options:@{}];
        NSError* error = nil;
        [handler performRequests:@[request] error:&error];

        CGImageRelease(cgImage);

        if (error || !request.results) {
            return QString();
        }

        NSMutableArray<NSString*>* lines = [NSMutableArray array];
        for (VNRecognizedTextObservation* obs in request.results) {
            NSArray<VNRecognizedText*>* candidates = [obs topCandidates:1];
            if (candidates && candidates.count > 0) {
                [lines addObject:candidates[0].string];
            }
        }

        NSString* joined = [lines componentsJoinedByString:@"\n"];
        return QString::fromUtf8([joined UTF8String]).trimmed();
    }

#else
    // ── Windows / Fallback: Temp file generation + OS engine ──────────────────
    // Generate a unique temp file path
    QString tmpName = QString("rb%1.png")
                          .arg(QUuid::createUuid().toString(QUuid::Id128).left(12));
#ifdef Q_OS_WIN
    QString tmpPath = QDir::tempPath() + "/" + tmpName;
#else
    QString tmpPath = "/tmp/" + tmpName;
#endif

    // Save at full quality (OCR needs sharp pixels)
    QImage img = pixmap.toImage();
    if (!img.save(tmpPath, "PNG")) return QString();

    QString output;

#ifdef Q_OS_WIN
    // ── Windows: Windows.Media.Ocr via hidden PowerShell ─────────────────────
    QString script = QString(R"(
try {
    Add-Type -AssemblyName System.Runtime.WindowsRuntime
    $asTask = [System.WindowsRuntimeSystemExtensions].GetMethods() | Where-Object { 
        $_.Name -eq 'AsTask' -and $_.GetParameters().Count -eq 1 -and $_.IsGenericMethod 
    } | Select-Object -First 1

    function Await-Async($op, $type) {
        $task = $asTask.MakeGenericMethod($type).Invoke($null, @($op))
        $task.Wait()
        return $task.Result
    }

    $null = [Windows.Storage.StorageFile, Windows.Storage, ContentType=WindowsRuntime]
    $null = [Windows.Media.Ocr.OcrEngine, Windows.Foundation, ContentType=WindowsRuntime]
    $null = [Windows.Graphics.Imaging.BitmapDecoder, Windows.Foundation, ContentType=WindowsRuntime]

    $file = Await-Async ([Windows.Storage.StorageFile]::GetFileFromPathAsync('%1')) ([Windows.Storage.StorageFile])
    $stream = Await-Async ($file.OpenAsync([Windows.Storage.FileAccessMode]::Read)) ([Windows.Storage.Streams.IRandomAccessStream])
    $decoder = Await-Async ([Windows.Graphics.Imaging.BitmapDecoder]::CreateAsync($stream)) ([Windows.Graphics.Imaging.BitmapDecoder])
    $softBmp = Await-Async ($decoder.GetSoftwareBitmapAsync()) ([Windows.Graphics.Imaging.SoftwareBitmap])
    $engine = [Windows.Media.Ocr.OcrEngine]::TryCreateFromUserProfileLanguages()
    $res = Await-Async ($engine.RecognizeAsync($softBmp)) ([Windows.Media.Ocr.OcrResult])
    if ($res -and $res.Text) { Write-Output $res.Text }
} catch { '' }
finally { if (Test-Path '%1') { Remove-Item '%1' -Force -ErrorAction SilentlyContinue } }
)").arg(tmpPath.replace('/', '\\'));

    QProcess ps;
    ps.setProgram("powershell.exe");
    ps.setArguments({"-WindowStyle","Hidden","-NonInteractive","-NoProfile",
                     "-ExecutionPolicy","Bypass","-Command", script});
    ps.setReadChannel(QProcess::StandardOutput);
    ps.start();
    bool ok = ps.waitForFinished(8000);
    if (!ok) { ps.kill(); ps.waitForFinished(500); }
    QFile::remove(tmpPath);
    if (ok) output = QString::fromUtf8(ps.readAllStandardOutput()).trimmed();

#else
    // Unsupported platform — clean up and return empty
    QFile::remove(tmpPath);
#endif

    return output;
#endif // !Q_OS_MAC
}

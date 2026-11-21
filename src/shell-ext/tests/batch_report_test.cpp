// Unit tests for the end-of-run report shown as a toast (issue #39).

#include "win11qc/batch_report.h"

#include <iostream>
#include <string>

using namespace win11qc::shell;

namespace {

int g_failures = 0;

void check(bool condition, const char* what)
{
    std::cout << (condition ? "PASS: " : "FAIL: ") << what << '\n';
    if (!condition) {
        ++g_failures;
    }
}

bool contains(const std::wstring& text, const std::wstring& part)
{
    return text.find(part) != std::wstring::npos;
}

BatchReport make_report(std::size_t attempted)
{
    BatchReport report;
    report.formatExt = L"png";
    report.attempted = attempted;
    return report;
}

} // namespace

int main()
{
    // --- should_notify ---
    {
        BatchReport single = make_report(1);
        single.outputs.push_back(L"C:\\pics\\a.png");
        check(!should_notify(single), "a successful single conversion stays silent");

        BatchReport singleFailed = make_report(1);
        singleFailed.failures.push_back({L"C:\\pics\\a.heic", L"Failed to open input image: 0x88982F50"});
        check(should_notify(singleFailed), "a failed single conversion is reported");

        BatchReport batch = make_report(3);
        batch.outputs = {L"C:\\pics\\a.png", L"C:\\pics\\b.png", L"C:\\pics\\c.png"};
        check(should_notify(batch), "a batch always gets a summary");

        check(!should_notify(make_report(0)), "nothing attempted, nothing shown");
    }

    // --- headlines and details ---
    {
        BatchReport batch = make_report(3);
        batch.outputs = {L"C:\\pics\\a.png", L"C:\\pics\\b.png", L"C:\\pics\\c.png"};
        const std::wstring xml = build_toast_xml(batch);
        check(contains(xml, L"<text>Converted 3 images to PNG</text>"), "all-success headline");
        check(contains(xml, L"<action content=\"Open folder\" activationType=\"protocol\" arguments=\"file:///C:/pics\"/>"),
              "Open folder action points at the shared output folder");
        check(!contains(xml, L"Open image"), "no Open image action for several outputs");
        check(contains(xml, L"<toast activationType=\"protocol\" launch=\"file:///C:/pics\">"),
              "clicking the toast opens the folder");
    }
    {
        BatchReport partial = make_report(4);
        partial.outputs = {L"C:\\pics\\a.png", L"C:\\pics\\b.png"};
        partial.failures = {{L"C:\\pics\\c.heic", L"Failed to open input image: 0x88982F50"},
                            {L"C:\\pics\\d.avif", L"Failed to open input image: 0x88982F50"}};
        const std::wstring xml = build_toast_xml(partial);
        check(contains(xml, L"<text>Converted 2 of 4 images to PNG</text>"), "partial-success headline");
        check(contains(xml, L"<text>c.heic: Failed to open input image: 0x88982F50</text>"), "first failure is named");
        check(contains(xml, L"<text>+1 more failed.</text>"), "remaining failures are counted");
    }
    {
        BatchReport allFailed = make_report(2);
        allFailed.failures = {{L"C:\\pics\\c.heic", L"x"}, {L"C:\\pics\\d.avif", L"y"}};
        const std::wstring xml = build_toast_xml(allFailed);
        check(contains(xml, L"<text>Couldn&apos;t convert 2 images to PNG</text>"), "all-failed headline");
        check(!contains(xml, L"<actions>") && !contains(xml, L"launch="), "no folder actions without outputs");
    }
    {
        BatchReport single = make_report(1);
        single.formatExt = L"webp";
        single.failures = {{L"C:\\pics\\a.heic", L"Failed to open input image: 0x88982F50"}};
        const std::wstring xml = build_toast_xml(single);
        check(contains(xml, L"<text>Couldn&apos;t convert a.heic to WebP</text>"), "single failure headline names the file");
        check(contains(xml, L"<text>Failed to open input image: 0x88982F50</text>"), "single failure shows the bare reason");
    }
    {
        BatchReport single = make_report(1);
        single.outputs = {L"C:\\my pics\\a#1.png"};
        const std::wstring xml = build_toast_xml(single);
        check(contains(xml, L"arguments=\"file:///C:/my%20pics/a%231.png\""), "Open image URI is percent-encoded");
    }
    {
        BatchReport cancelled = make_report(5);
        cancelled.cancelled = true;
        cancelled.outputs = {L"C:\\pics\\a.png", L"C:\\pics\\b.png"};
        const std::wstring xml = build_toast_xml(cancelled);
        check(contains(xml, L"<text>Conversion to PNG cancelled</text>"), "cancelled headline");
        check(contains(xml, L"<text>2 of 5 images converted.</text>"), "cancelled progress line");
    }
    {
        BatchReport documents = make_report(3);
        documents.formatExt = L"pdf";
        documents.outputs = {L"C:\\docs\\a.pdf", L"C:\\docs\\b.pdf"};
        documents.failures = {{L"C:\\docs\\c.docx", L"LibreOffice was not found."}};
        const std::wstring xml = build_toast_xml(documents);
        check(contains(xml, L"<text>Converted 2 of 3 documents to PDF</text>"), "document batches count documents");
    }
    {
        BatchReport document = make_report(1);
        document.formatExt = L"pdf";
        document.outputs = {L"C:\\docs\\a.pdf"};
        const std::wstring xml = build_toast_xml(document);
        check(contains(xml, L"Open PDF") && !contains(xml, L"Open image"), "single PDF gets an Open PDF action");
    }
    {
        BatchReport spread = make_report(2);
        spread.outputs = {L"C:\\a\\x.png", L"C:\\b\\y.png"};
        check(!contains(build_toast_xml(spread), L"Open folder"), "no Open folder when outputs span folders");
    }
    {
        BatchReport hostile = make_report(1);
        hostile.failures = {{L"C:\\pics\\<a&b>.png", L"bad \"quote\""}};
        const std::wstring xml = build_toast_xml(hostile);
        check(contains(xml, L"&lt;a&amp;b&gt;.png") && contains(xml, L"bad &quot;quote&quot;") && !contains(xml, L"<a&b>"),
              "file names and reasons are XML-escaped");
    }

    // --- cli_failure_reason ---
    check(cli_failure_reason("Input file does not exist: C:\\x.png\r\n", 2) == L"Input file does not exist: C:\\x.png",
          "uses the CLI's message line");
    check(cli_failure_reason("\r\nUnknown argument: --bogus\nRun with --help for usage information.\n", 1) ==
              L"Unknown argument: --bogus",
          "takes the first non-empty line");
    check(cli_failure_reason("Input file does not exist: C:\\\xE7\x94\xBB\xE5\x83\x8F.png", 2) ==
              L"Input file does not exist: C:\\\x753B\x50CF.png",
          "decodes UTF-8 output");
    check(cli_failure_reason("", 3) == L"converter-cli exited with code 3.", "falls back to the exit code");

    return g_failures == 0 ? 0 : 1;
}

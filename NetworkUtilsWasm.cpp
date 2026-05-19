#include <QString>

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#include <cstdlib>

EM_JS(char *, network_utils_wasm_page_hostname, (), {
    var host = (typeof location !== 'undefined' && location.hostname) ? location.hostname : '';
    var len = lengthBytesUTF8(host) + 1;
    var buf = _malloc(len);
    stringToUTF8(host, buf, len);
    return buf;
});

QString networkUtilsWasmPageHostname()
{
    char *hostC = network_utils_wasm_page_hostname();
    const QString hostname = hostC ? QString::fromUtf8(hostC) : QString();
    if (hostC)
        std::free(hostC);
    return hostname;
}
#endif

// G2B file drag-out: sleep een knop in de UI -> Windows-bestand-drag (CF_HDROP).
// Werkt op Windows (OLE). Op Linux/macOS een no-op die false teruggeeft.

#include <stdint.h>

#if defined(DISTRHO_OS_WINDOWS) || defined(_WIN32) || defined(__MINGW32__)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ole2.h>
#include <shlobj.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <string>

namespace {

class CDropSource : public IDropSource {
public:
    ULONG AddRef() override { return ++ref_; }
    ULONG Release() override { ULONG r = --ref_; if (!r) delete this; return r; }
    HRESULT QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_IDropSource) { *ppv = this; AddRef(); return S_OK; }
        *ppv = nullptr; return E_NOINTERFACE;
    }
    HRESULT QueryContinueDrag(BOOL esc, DWORD keys) override {
        if (esc) return DRAGDROP_S_CANCEL;
        if (!(keys & MK_LBUTTON)) return DRAGDROP_S_DROP;
        return S_OK;
    }
    HRESULT GiveFeedback(DWORD) override { return DRAGDROP_S_USEDEFAULTCURSORS; }
private:
    ULONG ref_ = 1;
};

class CDataObject : public IDataObject {
public:
    explicit CDataObject(const std::wstring& path) {
        // DROPFILES + wide path + dubbele nul-terminator
        const size_t bytes = sizeof(DROPFILES) + (path.size() + 2) * sizeof(wchar_t);
        hg_ = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, bytes);
        if (!hg_) return;
        char* p = static_cast<char*>(GlobalLock(hg_));
        DROPFILES* df = reinterpret_cast<DROPFILES*>(p);
        df->pFiles = sizeof(DROPFILES);
        df->fWide = TRUE;
        wchar_t* w = reinterpret_cast<wchar_t*>(p + sizeof(DROPFILES));
        memcpy(w, path.c_str(), path.size() * sizeof(wchar_t));
        GlobalUnlock(hg_);
    }
    ~CDataObject() { if (hg_) GlobalFree(hg_); }

    ULONG AddRef() override { return ++ref_; }
    ULONG Release() override { ULONG r = --ref_; if (!r) delete this; return r; }
    HRESULT QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_IDataObject) { *ppv = this; AddRef(); return S_OK; }
        *ppv = nullptr; return E_NOINTERFACE;
    }
    HRESULT GetData(FORMATETC* fe, STGMEDIUM* med) override {
        if (!fe || !med || fe->cfFormat != CF_HDROP || !(fe->tymed & TYMED_HGLOBAL) || !hg_)
            return DV_E_FORMATETC;
        med->tymed = TYMED_HGLOBAL;
        med->hGlobal = hg_;
        med->pUnkForRelease = this;
        AddRef(); // target released via ReleaseStgMedium -> pUnkForRelease
        return S_OK;
    }
    HRESULT QueryGetData(FORMATETC* fe) override {
        return (fe && fe->cfFormat == CF_HDROP && (fe->tymed & TYMED_HGLOBAL) && hg_) ? S_OK : DV_E_FORMATETC;
    }
    HRESULT GetDataHere(FORMATETC*, STGMEDIUM*) override { return E_NOTIMPL; }
    HRESULT GetCanonicalFormatEtc(FORMATETC*, FORMATETC* out) override { if (out) out->ptd = nullptr; return E_NOTIMPL; }
    HRESULT SetData(FORMATETC*, STGMEDIUM*, BOOL) override { return E_NOTIMPL; }
    HRESULT EnumFormatEtc(DWORD dir, IEnumFORMATETC** pp) override {
        if (dir != DATADIR_GET || !pp) return E_NOTIMPL;
        FORMATETC fe = { CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
        return SHCreateStdEnumFmtEtc(1, &fe, pp);
    }
    HRESULT DAdvise(FORMATETC*, DWORD, IAdviseSink*, DWORD*) override { return OLE_E_ADVISENOTSUPPORTED; }
    HRESULT DUnadvise(DWORD) override { return OLE_E_ADVISENOTSUPPORTED; }
    HRESULT EnumDAdvise(IEnumSTATDATA**) override { return OLE_E_ADVISENOTSUPPORTED; }
private:
    ULONG ref_ = 1;
    HGLOBAL hg_ = nullptr;
};

} // namespace

extern "C" int g2bStartFileDrag(uintptr_t hwndNative, const char* utf8Path)
{
    if (!utf8Path || !*utf8Path) return 0;

    // pad naar wide
    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8Path, -1, nullptr, 0);
    if (wlen <= 0) return 0;
    std::wstring wpath(wlen - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8Path, -1, &wpath[0], wlen);
    for (auto& c : wpath) if (c == L'/') c = L'\\';

    HRESULT hr = OleInitialize(nullptr);
    const bool ole = SUCCEEDED(hr);
    DWORD effect = 0;
    HRESULT res = E_FAIL;
    {
        CDataObject* data = new CDataObject(wpath);
        CDropSource* src = new CDropSource();
        res = DoDragDrop(data, src, DROPEFFECT_COPY, &effect);
        src->Release();
        data->Release();
    }
    if (ole) OleUninitialize();
    return SUCCEEDED(res) ? 1 : 0;
}

#else

extern "C" int g2bStartFileDrag(uintptr_t, const char*) { return 0; }

#endif

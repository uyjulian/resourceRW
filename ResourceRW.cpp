#include <windows.h>
#include "tp_stub.h"
#include "simplebinder.hpp"

#include "ResourceRW.hpp"

iTJSDispatch2* ResourceUtil::LangIdTable = NULL;
bool ResourceUtil::SetupLangIdTable(bool link) {
	if (!link) {
		if (!LangIdTable) return false; // already unlinked
		LangIdTable->Release();
		LangIdTable = NULL;
		return true;
	} else if (LangIdTable) return false; // already linked

	LangIdTable = TJSCreateDictionaryObject();
	SimpleBinder::BindUtil(LangIdTable, link)
#define _DEFLANG_(tag) .Variant(TJS_W( #tag), tag)
#include "lang.inc"
#undef  _DEFLANG_
;
	return true;
}

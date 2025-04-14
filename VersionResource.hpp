#pragma once

#include <windows.h>
#include <vector>
#include <list>
#include <string>
#include <cstdint>
#include <cctype>

#if defined(__cplusplus) && (__cplusplus >= 201103L)
#define METHOD_DELETE = delete
#define U16TEXT(TEXT) (u ## TEXT)
#else
#define METHOD_DELETE
#define U16TEXT(TEXT) ((const char16_t*)(L ## TEXT))
static_assert(sizeof(wchar_t) == 2, "cannot use U16TEXT macro");
#endif

namespace VersionResource {

using std::size_t;
using std::u16string;
typedef std::u16string::value_type char16_t;

#if defined(__cplusplus) && (__cplusplus >= 201103L)
template<class T>
using ChildList = std::list<T>;
#else
template<class T>
struct ChildList : public std::list<T> {};
#endif

// 処理範囲
struct VsRange {
	size_t begin, end;
};

//--------------------------------------------------------------
// テーブル読み書き操作
struct BlobReader {
	BlobReader(const BYTE *src) : src(src) {}
	bool alignment(VsRange &range) const {
		const size_t fix = (range.begin & 3);
		if (fix != 0) range.begin += (4-fix);
		return range.begin <= range.end;
	}
	bool copy(void *ptr, const VsRange &pos, const VsRange &range) const {
		if ((pos.begin < pos.end) &&
			(pos.end <= range.end) &&
			(range.begin <= pos.begin))
		{
			memcpy(ptr, src + pos.begin, pos.end - pos.begin);
			return true;
		}
		return false;
	}
	int string(u16string &str, const VsRange &range) const {
		if ((range.begin & 1) || (range.begin > range.end)) return -1;
		const WCHAR *ptr = reinterpret_cast<const WCHAR*>(src + range.begin);
		const WCHAR *end = reinterpret_cast<const WCHAR*>(src + range.end);
		const WCHAR * const top = ptr;
		for (str.clear(); ptr < end && *ptr; ++ptr) str.push_back((char16_t)*ptr);
		return (ptr < end) ? ((int(ptr - top) + 1) * sizeof (WCHAR)) : -1;
	}
private:
	const BYTE *src;
};
struct BlobSizer {
	BlobSizer() : total(0) {}
	size_t position() const { return total; }
	void padding() {
		const size_t fix = (total & 3);
		if (fix != 0) total += (4-fix);
	}
	void append(const void *ptr, size_t len) { total += len; }
	size_t total;
};
template <typename T>
struct BlobWriter {
	BlobWriter(std::vector<T> &data) : data(data) {}
	size_t position() const { return data.size(); }
	void padding() {
		const size_t fix = (data.size() & 3);
		if (fix != 0) {
			T padding[3] = {0,0,0};
			append(padding, 4 - fix);
		}
	}
	void append(const void *ptr, size_t len) {
		auto src = reinterpret_cast<const T*>(ptr);
		std::copy(src, src+len, std::back_inserter(data));
	}
	std::vector<T> &data;
};
//--------------------------------------------------------------
// VS値・テーブル共通部・操作ユーティリティ

/*
struct NonCopyable {
	NonCopyable() {}
protected:
	NonCopyable(const NonCopyable&) METHOD_DELETE ;
	NonCopyable& operator=(const NonCopyable&) METHOD_DELETE ;
};
 */
struct VsBase { //: protected NonCopyable {
	WORD  wLength;      // The length, in bytes, of this self structure, including all structures indicated by the Children member if exists.
	WORD  wValueLength; // The length, in bytes, of the Value member if Value member exists. otherwise equal to zero.
	WORD  wType;        // The type of data in the version resource. This member is 1 if the version resource contains text data and 0 if the version resource contains binary data.
	u16string szKey;
//	WORD  Padding;      // As many zero words as necessary to align the next member on a 32-bit boundary.

	VsBase() : wLength(0), wValueLength(0), wType(0) { szKey.clear(); }
	void clear(WORD type = 0) {
		wLength = wValueLength = 0;
		wType = type;
		szKey.clear();
	}

	template <class T>
	int load(T &blob, const VsRange &range) {
		VsRange valpos(range);
		const VsRange head = { range.begin, range.begin + sizeof(WORD)*3 };
		if (!blob.copy(this, head, range)) return -1;
		valpos.begin = head.end;
		const int step = blob.string(szKey, valpos);
		if (step < 0) return -1;
		valpos.begin += step;
		return blob.alignment(valpos) ? (valpos.begin - range.begin) : -1;
	}
	template <class T>
	void save(T &blob) {
		blob.append(this, sizeof(WORD)*3);
		SaveString(szKey, blob);
		blob.padding();
	}

	bool repos(VsRange &pos, const VsRange &range) const {
		pos.end = range.begin + wLength;
		return pos.end <= range.end;
	}
	int next(const VsRange &pos, const VsRange &range) const {
		const int step = (pos.begin - range.begin);
		return (step == (int)wLength) ? step : -1;
	}

	template <typename V>
	static V* FindChildren(ChildList<V> &Children, const u16string &key) {
		for (auto it = Children.begin(); it != Children.end(); ++it) if (it->szKey == key) return &(*it);
		return nullptr;
	}
	template <typename V>
	static const V* FindChildren(const ChildList<V> &Children, const u16string &key) {
		for (auto it = Children.cbegin(); it != Children.cend(); ++it) if (it->szKey == key) return &(*it);
		return nullptr;
	}

	template <typename V>
	static typename ChildList<V>::iterator FindChildrenIC(ChildList<V> &Children, const u16string &key) {
		for (auto it = Children.begin(); it != Children.end(); ++it) {
			if (key.size() == it->szKey.size()
				&& std::equal(key.cbegin(), key.cend(), it->szKey.cbegin(),
							  [](char16_t a, char16_t b) {
								  return std::tolower(a) == std::tolower(b);
							  })) return it;
		}
		return Children.end();
	}

	template <typename T>
	static bool LoadHead(VsBase *self, T &blob, VsRange &valpos) {
		const int step = self->load(blob, valpos);
		if (step < 0) return false;
		valpos.begin += step;
		return blob.alignment(valpos);
	}

	template <typename V, typename T>
	static int LoadChildren(ChildList<V> &Children, T &blob, VsRange &valpos) {
		const size_t begin = valpos.begin;
		while (valpos.begin < valpos.end) {
			if (!blob.alignment(valpos)) return -1;
			Children.push_back(V());
			const int step = Children.back().load(blob, valpos);
			if (step < 0) {
				Children.pop_back();
				return step;
			}
			valpos.begin += step;
		}
		return (valpos.begin - begin);
	}

	template <typename T>
	static bool LoadString(u16string &str, T &blob, VsRange &valpos) {
		const int step = blob.string(str, valpos);
		if (step < 0) return false;
		valpos.begin += step;
		return true;
	}

	template <typename V, typename T>
	static void SaveChildren(ChildList<V> &Children, T &blob, bool update) {
		for (auto it = Children.begin(); it != Children.end(); ++it) {
			blob.padding();
			it->save(blob, update);
		}
	}

	template <typename T>
	static void SaveString(const u16string &str, T &blob) {
		const WCHAR zero = 0;
		blob.append(str.c_str(), str.length() * sizeof(char16_t));
		blob.append(&zero, sizeof(zero)); // zero-terminator
		//blob.padding();
	}

	static const char16_t* ToHexString(DWORD num, char16_t str[9]) {
		static const char16_t hex[] = {0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39, 0x61,0x62,0x63,0x64,0x65,0x66};
		for (int n = 7; n >=0; --n, num>>=4) str[n] = hex[num & 0xF];
		str[8] = 0;
		return str;
	}

	template <typename T>
	struct LengthFiller {
		LengthFiller(VsBase &target, const T &blob, bool update) : target(target), blob(blob), start(blob.position()), update(update) {}
		~LengthFiller() { if (update) target.wLength = blob.position() - start; }
	private:
		VsBase &target;
		const T &blob;
		const size_t start;
		const bool update;
	};
};

//--------------------------------------------------------------
// \StringFileInfo\{LANGCODEPAGE}\Value値
struct String : public VsBase {
//	WORD  Value;
	u16string Value;      // A zero-terminated string.

	String() { clear(); }
	void clear() {
		VsBase::clear(1); // type:str
		Value.clear();
	}

	template <class T>
	int load(T &blob, const VsRange &range) {
		clear();

		VsRange valpos(range);
		if (!LoadHead(this, blob, valpos) || !repos(valpos, range)) return -1;
		if (wValueLength > 0) {
			if (!LoadString(Value, blob, valpos)) return -1;
		} else {
			Value.clear();
		}
		return next(valpos, range);
	}
	template <class T>
	void save(T &blob, bool update) {
		LengthFiller<T> fill(*this, blob, update);
		if (update) {
			wValueLength = (Value.length() + 1); // [MEMO] byte数ではなく文字数(\0含める)
//			wType = 1; // type:str
		}
		VsBase::save(blob);
		SaveString(Value, blob);
	}
};
//--------------------------------------------------------------
// \StringFileInfo\{LANGCODEPAGE} テーブル
struct StringTable : public VsBase {
	ChildList<String> Children; // An array of one or more String structures.

	StringTable() { clear(); }
	void clear() {
		VsBase::clear(1); // type:str
		Children.clear();
	}
	void reset(DWORD lang) {
		clear();
		changeLang(lang);
	}
	void changeLang(DWORD lang) {
		char16_t key[9];
		szKey = ToHexString(lang, key);
	}

	bool change(const u16string &key, const u16string &val) {
		String *str = FindChildren(Children, key);
		if (str) str->Value = val;
		else {
			Children.push_back(String());
			Children.back().szKey = key;
			Children.back().Value = val;
		}
		return true;
	}
	

	template <class T>
	int load(T &blob, const VsRange &range) {
		clear();

		VsRange valpos(range);
		if (!LoadHead(this, blob, valpos) ||
			!repos(valpos, range) ||
			(LoadChildren(Children, blob, valpos) < 0)) return -1;
		return next(valpos, range);
	}
	template <class T>
	void save(T &blob, bool update) {
		LengthFiller<T> fill(*this, blob, update);
		if (update) {
			wValueLength = 0;
//			wType = 1; // type:str
		}
		VsBase::save(blob);
		SaveChildren(Children, blob, update);
	}
};
//--------------------------------------------------------------
// \VarFileInfo\Translation
struct Var : public VsBase {
//	ChildList<DWORD> Value;
	std::vector<DWORD> Value;

	Var() { clear(); }
	void clear() {
		VsBase::clear(0); // type:bin
		Value.clear();
	}
	void reset(DWORD lang) {
		clear();
		szKey = U16TEXT("Translation");
		Value.push_back(lang);
	}

	size_t getTranslations(std::vector<DWORD> &langs) const {
		langs.assign(Value.cbegin(), Value.cend());
		return Value.size();
	}

	bool addnew(DWORD lang) {
		auto it = std::find(Value.cbegin(), Value.cend(), lang);
		if (it != Value.cend()) return false; // 既に存在する
		Value.push_back(lang);
		return true;
	}
	bool remove(DWORD lang) {
		auto it = std::find(Value.begin(), Value.end(), lang);
		if (it == Value.end()) return false; // 削除対象が無い
		Value.erase(it);
		return true;
	}

	template <class T>
	int load(T &blob, const VsRange &range) {
		clear();

		VsRange valpos(range);
		if (!LoadHead(this, blob, valpos) || !repos(valpos, range)) return -1;
		const size_t cnt = wValueLength>>2; // wValueLength/4
		Value.resize(cnt);
		if (cnt > 0) {
			const VsRange pos = { valpos.begin, valpos.begin + sizeof(DWORD)*cnt };
			if (!blob.copy(&Value.front(), pos, valpos)) return -1;
			for (auto it = Value.begin(); it != Value.end(); ++it) {
				const DWORD val = *it;
				*it = ((val << 16) | (val >> 16)); // [XXX] WORD Swapping / 直コピーだとエンディアンの関係でWORD順が逆になるため
			}
			valpos.begin = pos.end;
		}
		return next(valpos, range);
	}
	template <class T>
	void save(T &blob, bool update) {
		LengthFiller<T> fill(*this, blob, update);
		if (update) {
			wValueLength = Value.size() * sizeof(DWORD);
//			wType = 0; // type:bin
		}
		VsBase::save(blob);
		const size_t cnt = Value.size();
		if (cnt > 0) {
			for (auto it = Value.cbegin(); it != Value.cend(); ++it) {
				const DWORD val = *it;
				const WORD words[] = { (WORD)(val >> 16), (WORD)val };
				blob.append(words, sizeof(words));
			}
		}
	}
};
//--------------------------------------------------------------
// \StringFileInfo | \VarFileInfo テーブル
struct FileInfo : public VsBase {
	int childrenType;
	ChildList<StringTable> StringChildren; // An array of one or more StringTable structures. Each StringTable structure's szKey member indicates the appropriate language and code page for displaying the text in that StringTable structure.
	ChildList<Var> VarChildren;  // Typically contains a list of languages that the application or DLL supports.

	FileInfo() { clear(); }
	void clear() {
		VsBase::clear(1); // type:str
		childrenType = 0;
		StringChildren.clear();
		VarChildren.clear();
	}

	void reset(int type, DWORD lang) {
		clear();
		childrenType = type;
		if (type > 0) {
			szKey = U16TEXT("StringFileInfo");
			StringChildren.push_back(StringTable());
			StringChildren.back().reset(lang);
		} else if (type < 0) {
			szKey = U16TEXT("VarFileInfo");
			VarChildren.push_back(Var());
			VarChildren.back().reset(lang);
		}
	}

	bool changeString(const u16string &key, const u16string &val, DWORD lang) {
		char16_t hex[9] = {0};
		auto tbl = FindChildrenIC(StringChildren, ToHexString(lang, hex));
		if (tbl == StringChildren.end()) return false;
		return tbl->change(key, val);
	}
	bool changeTranslation(const DWORD *srclang, const DWORD *dstlang) {
		if (!srclang && !dstlang) return false;
		const bool addnew = (srclang == nullptr && dstlang != nullptr);
		const bool remove = (srclang != nullptr && dstlang == nullptr);
//		const bool copy   = (srclang != nullptr && dstlang != nullptr);
		if (childrenType > 0) {
			char16_t srchex[9] = {0}, dsthex[9] = {0};
			auto const term = StringChildren.end();
			auto srctbl = srclang ? FindChildrenIC(StringChildren, ToHexString(*srclang, srchex)) : term;
			auto dsttbl = dstlang ? FindChildrenIC(StringChildren, ToHexString(*dstlang, dsthex)) : term;
			if (addnew) {
				if (dsttbl == term) return false; // 追加先が既に存在する
				StringChildren.push_back(StringTable());
				StringChildren.back().reset(*dstlang);
				return true;
			} else if (remove) {
				if (srctbl == term) return false; // 削除先が無い
				StringChildren.erase(srctbl);
				return true;
			} else {
				if (srctbl == term) return false; // コピー元が無い
				if (dsttbl != term) return false; // コピー先が既に存在する
				StringChildren.push_back(*srctbl);
				StringChildren.back().changeLang(*dstlang);
				return true;
			}
		} else if (childrenType < 0) {
			Var *var = FindChildren(VarChildren, U16TEXT("Translation"));
			if (!var) return false;
			if (remove) return var->remove(*srclang);
			else        return var->addnew(*dstlang); // コピーはaddと同じ挙動
		}
		return false;
	}
	size_t getTranslations(std::vector<DWORD> &langs) const {
		const Var *var = FindChildren(VarChildren, U16TEXT("Translation"));
		return var ? var->getTranslations(langs) : 0;
	}

	template <class T>
	int load(T &blob, const VsRange &range) {
		clear();

		VsRange valpos(range);
		if (!LoadHead(this, blob, valpos) || !repos(valpos, range)) return -1;
		childrenType = (szKey == U16TEXT("StringFileInfo")) ? 1 : (szKey == U16TEXT("VarFileInfo")) ? -1 : 0;
		int step = 0;
		if      (childrenType > 0) step = LoadChildren(StringChildren, blob, valpos);
		else if (childrenType < 0) step = LoadChildren(   VarChildren, blob, valpos);
		return step < 0 ? step : next(valpos, range);
	}
	template <class T>
	void save(T &blob, bool update) {
		LengthFiller<T> fill(*this, blob, update);
		if (update) {
			wValueLength = 0;
//			wType = 1; // type:str
		}
		VsBase::save(blob);
		if      (childrenType > 0) SaveChildren(StringChildren, blob, update);
		else if (childrenType < 0) SaveChildren(   VarChildren, blob, update);
	}
};
//--------------------------------------------------------------
// VS_VERSION_INFO テーブル
struct VersionInfo : public VsBase {
	struct VS_FIXEDFILEINFO {
		DWORD dwSignature;
		DWORD dwStrucVersion;
		DWORD dwFileVersionMS;
		DWORD dwFileVersionLS;
		DWORD dwProductVersionMS;
		DWORD dwProductVersionLS;
		DWORD dwFileFlagsMask;
		DWORD dwFileFlags;
		DWORD dwFileOS;
		DWORD dwFileType;
		DWORD dwFileSubtype;
		DWORD dwFileDateMS;
		DWORD dwFileDateLS;
	} Value;
//	WORD  Padding;      // As many zero words as necessary to align the next member on a 32-bit boundary.
	ChildList<FileInfo> Children;

	VersionInfo() { clear(); }
	void clear() {
		VsBase::clear(0); // type:bin
		::ZeroMemory(&Value, sizeof(Value));
		Children.clear();
	}
	void reset(DWORD lang) {
		clear();
		szKey = U16TEXT("VS_VERSION_INFO");
		Value.dwSignature = 0xFEEF04BD;
		Value.dwFileFlagsMask = 0x3F;
		Value.dwFileFlags     = 0x10; // VS_FF_INFOINFERRED [ファイルのバージョン構造は動的に作成されました。したがって、この構造体の一部のメンバーは空であるか、正しくない可能性があります。]
		Value.dwFileOS        = 0x40004L; // VOS_NT | VOS_WINDOWS32
		Children.push_back(FileInfo());
		Children.push_back(FileInfo());
		Children.front().reset( 1, lang);
		Children.back() .reset(-1, lang);
	}
	bool changeString(const u16string &key, const u16string &val, DWORD lang) {
		FileInfo *str = FindChildren(Children, U16TEXT("StringFileInfo"));
		return str && str->changeString(key, val, lang);
	}
	bool changeTranslation(const DWORD *srclang, const DWORD *dstlang) {
		if (!srclang && !dstlang) return false;
		FileInfo *str = FindChildren(Children, U16TEXT("StringFileInfo"));
		FileInfo *var = FindChildren(Children, U16TEXT("VarFileInfo"));
		if (str && var) {
			const bool rstr = str->changeTranslation(srclang, dstlang);
			if (!rstr) return false;
			const bool rvar = var->changeTranslation(srclang, dstlang);
			if (!rvar) {
				if (dstlang) str->changeTranslation(dstlang, 0); // 追加済みの場合は削除
				return false;
			}
			// 新規追加時に VS_FF_INFOINFERRED フラグを立てる
			if (!srclang && dstlang) {
				Value.dwFileFlagsMask |= 0x10;
				Value.dwFileFlags     |= 0x10;
			}
			return true;
		}
		return false;
	}

	size_t getTranslations(std::vector<DWORD> &langs) const {
		const FileInfo *var = FindChildren(Children, U16TEXT("VarFileInfo"));
		return var ? var->getTranslations(langs) : 0;
	}

	bool changeFileInfo(const u16string &key, uint64_t val) {
		/**/ if (key == U16TEXT("Signature"))      Value.dwSignature     = (DWORD)val;
		else if (key == U16TEXT("StrucVersion"))   Value.dwStrucVersion  = (DWORD)val;
		else if (key == U16TEXT("FileFlagsMask"))  Value.dwFileFlagsMask = (DWORD)val;
		else if (key == U16TEXT("FileFlags"))      Value.dwFileFlags     = (DWORD)val;
		else if (key == U16TEXT("FileOS"))         Value.dwFileOS        = (DWORD)val;
		else if (key == U16TEXT("FileType"))       Value.dwFileType      = (DWORD)val;
		else if (key == U16TEXT("FileSubtype"))    Value.dwFileSubtype   = (DWORD)val;
		else if (key == U16TEXT("FileVersion"))    SetValueMSLS(val, Value.dwFileVersionMS,    Value.dwFileVersionLS);
		else if (key == U16TEXT("ProductVersion")) SetValueMSLS(val, Value.dwProductVersionMS, Value.dwProductVersionLS);
		else if (key == U16TEXT("FileDate"))       SetValueMSLS(val, Value.dwFileDateMS,       Value.dwFileDateLS);
		else return false;
		return true;
	}
	static void SetValueMSLS(uint64_t val, DWORD &MS, DWORD &LS) { MS = (DWORD)(val>>32); LS = (DWORD)val; }

	template <class T>
	int load(T &blob, const VsRange &range) {
		clear();

		VsRange valpos(range);
		if (!LoadHead(this, blob, valpos)) return -1;

		valpos.end = valpos.begin + sizeof(Value);
		if (!blob.copy(&Value, valpos, range)) return -1;
		if (Value.dwSignature != 0xFEEF04BD) return -1; 

		valpos.begin = valpos.end;
		if (!repos(valpos, range) || (LoadChildren(Children, blob, valpos) < 0)) return -1;
		return next(valpos, range);
	}
	template <class T>
	void save(T &blob, bool update) {
		LengthFiller<T> fill(*this, blob, update);
		if (update) {
			wValueLength = sizeof(Value);
//			wType = 0; // type:bin
		}
		VsBase::save(blob);
		blob.append(&Value, sizeof(Value));
		SaveChildren(Children, blob, update);
	}
};

///////////////////////////////////////////////////////////////////////////////
// publicクラス

class VersionContainer {
	VersionInfo info_;
public:
	VersionContainer() {}
	~VersionContainer() {}

	void clear() {
		info_.clear();
	}
	void reset(DWORD lang = 0x041104b0L) { // Japanese - unicode
		info_.reset(lang);
	}

	// \{VS_FIXEDFILEINFO:key} = val
	bool changeFileInfo(const u16string &key, uint64_t val) {
		return info_.changeFileInfo(key, val);
	}

	// \StringFileInfo\{lang}\key = val
	bool changeString(const u16string &key, const u16string &val, DWORD lang) {
		return info_.changeString(key, val, lang);
	}
	// copy srclang to dstlang / remove srclang (dstlang==nullptr) / create dstlang (srclang==nullptr)
	bool changeTranslation(const DWORD *srclang, const DWORD *dstlang) {
		return info_.changeTranslation(srclang, dstlang);
	}
	// get Translation lang-codepage list
	size_t getTranslations(std::vector<DWORD> &langs) const {
		return info_.getTranslations(langs);
	}

	bool load(const BYTE *ptr, size_t len) {
		const VsRange range = { 0, len };
		const BlobReader reader(ptr);
		const int result = info_.load(reader, range);
		return (result > 0) && (len == (size_t)result);
	}

	template <typename T>
	void save(std::vector<T> &vec) {
		static_assert(sizeof (T) == 1, "sizeof(T) == 1");

		BlobSizer sizer;
		info_.save(sizer, true);

		const size_t total = sizer.total;
		vec.clear();
		vec.reserve(total);
		BlobWriter<T> writer(vec);
		info_.save(writer, false);
	}
};


} // namespace VersionResource

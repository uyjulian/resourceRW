#pragma once

#include <windows.h>
#include <vector>

namespace IconResource {

#pragma pack(push)
#pragma pack(2)

struct ICONHDR {
	BYTE	bWidth;					// Width, in pixels, of the image
	BYTE	bHeight;				// Height, in pixels, of the image
	BYTE	bColorCount;			// Number of colors in image (0 if >=8bpp)
	BYTE	bReserved;				// Reserved ( must be 0)
	union {
		struct {
			WORD	wPlanes;				// Color Planes
			WORD	wBitCount;				// Bits per pixel
		} ico;
		struct {
			WORD	wHotSpotX;
			WORD	wHotSpotY;
		} cur;
	};
};
struct ICONDIRENTRY {
	ICONHDR	ih;
	DWORD	dwBytesInRes;			// How many bytes in this resource?
	DWORD	dwImageOffset;			// Where in the file is this image?
};

struct GRPICONDIRENTRY {
	ICONHDR	ih;
	DWORD	dwBytesInRes;			// how many bytes in this resource?
	WORD	nID;					// the ID
};

struct ICONDIR {
	WORD	idReserved;				// Reserved (must be 0)
	WORD	idType;					// Resource Type (1 for icons)
	WORD	idCount;				// How many images?
};

#pragma pack(pop)

class ImageContainer {
public:
	typedef std::vector<BYTE> IconImage;
	typedef ICONDIRENTRY IconEntry;
private:
	ICONDIR dir_;
	struct Entry {
		Entry() : id(-1) { ::ZeroMemory(&icon, sizeof(icon)); }
		Entry(const BYTE *src) : id(-1) {
			memcpy(&icon, src, sizeof(icon));
		}
		int id;
		IconEntry icon;
		IconImage image;
	};
	std::vector<Entry> entries_;

	const Entry* getEntry(size_t index) const { return (index < getCount()) ? &entries_[index] : nullptr; }
	/**/  Entry* getEntry(size_t index)       { return (index < getCount()) ? &entries_[index] : nullptr; }

	struct ImageHeader {
		DWORD width, height;
		WORD planes, bits;
	};
	static inline DWORD ToU32BE(const BYTE *p) {
		return (((DWORD)p[0] << 24) |
				((DWORD)p[1] << 16) |
				((DWORD)p[2] <<  8) |
				((DWORD)p[3]      ));
	}
	static bool FetchPNGHeader(const BYTE *img, size_t len, ImageHeader &head) {
		static const BYTE png[] = {
			0x89, /*P*/0x50, /*N*/0x4E, /*G*/0x47, 0x0D, 0x0A, 0x1A, 0x0A,
			0x00, 0x00, 0x00, 0x0D, /*I*/0x49, /*H*/0x48, /*D*/0x44, /*R*/0x52
		};
		if (len < (sizeof(png) + (4+4+5)) || memcmp(img, png, sizeof(png)) != 0) return false;
		head.width  = ToU32BE(img + 16);
		head.height = ToU32BE(img + 20);
		head.planes = 1;
		head.bits = (WORD)img[24];
		const BYTE type = img[25];
		if      (type & 0x04) head.bits = 32;
		else if (type & 0x02) head.bits = 24;
		return true;
	}
	static bool FetchBMPHeader(const BYTE *img, size_t len, ImageHeader &head) {
		if (len < sizeof(DWORD)) return false;
		DWORD headsize = *reinterpret_cast<const DWORD*>(img);
		if (headsize == sizeof(BITMAPCOREHEADER)) {
			auto *core = reinterpret_cast<const BITMAPCOREHEADER*>(img);
			head.width  = core->bcWidth;
			head.height = core->bcHeight;
			head.planes = core->bcPlanes;
			head.bits   = core->bcBitCount;
		} else if (headsize >= sizeof(BITMAPINFOHEADER)) {
			auto *bmp  = reinterpret_cast<const BITMAPINFOHEADER*>(img);
			head.width  = bmp->biWidth;
			head.height = bmp->biHeight;
			head.planes = bmp->biPlanes;
			head.bits   = bmp->biBitCount;
		} else return false;
		return true;
	}
	static void ConvertEntry(const Entry &ent, IconEntry &icon, bool iscursor) {
		const size_t len = ent.image.size();
		const BYTE *img = len > 0 ? &ent.image.front() : nullptr;
		ImageHeader head;
		if (FetchPNGHeader(img, len, head)) {
			if (head.width < 256 && head.height < 256) {
				icon.ih.bWidth  = (BYTE)head.width;
				icon.ih.bHeight = (BYTE)head.height;
			} else {
				icon.ih.bWidth  = 0;
				icon.ih.bHeight = 0;
			}
			icon.ih.bColorCount = 0;
			icon.ih.bReserved   = 0;
			if (iscursor) {
				// copy hotspot
				icon.ih.cur.wHotSpotX = ent.icon.ih.cur.wHotSpotX;
				icon.ih.cur.wHotSpotY = ent.icon.ih.cur.wHotSpotY;
			} else {
				icon.ih.ico.wPlanes   = head.planes;
				icon.ih.ico.wBitCount = head.bits;
			}
			icon.dwBytesInRes  = ent.icon.dwBytesInRes;
			icon.dwImageOffset = ent.icon.dwImageOffset;
		} else {
			memcpy(&icon, &ent.icon, sizeof(icon));
			if (FetchBMPHeader(img, len, head)) {
				if (head.width < 256 && head.height < 256) {
					icon.ih.bWidth  = (BYTE)head.width;
					icon.ih.bHeight = (BYTE)head.height;
				}
				if (!iscursor) {
					icon.ih.ico.wPlanes   = head.planes;
					icon.ih.ico.wBitCount = head.bits;
				}
			}
		}
	}
public:
	ImageContainer() {}
	~ImageContainer() { clear(); }

	void clear() {
		::ZeroMemory(&dir_, sizeof(dir_));
		entries_.clear();
	}
	void reset(const ICONDIR &dir, size_t count) {
		clear();
		memcpy(&dir_, &dir, sizeof(dir_));
		entries_.resize(count);
	}

	const ICONDIR& getDir() const { return dir_; }
	bool isCursor() const { return dir_.idType == 2; }

	size_t getCount() const { return entries_.size(); }

	bool getID(size_t index, int &id) const { const Entry *ent = getEntry(index); if (ent) id = ent->id; return ent != nullptr; }
	bool setID(size_t index, int  id)       {       Entry *ent = getEntry(index); if (ent) ent->id = id; return ent != nullptr; }

	bool getIcon(size_t index, IconEntry &icon,             int *id = nullptr) const { const Entry *ent = getEntry(index); if (ent) { if(id) *id=ent->id; ConvertEntry(*ent, icon, isCursor());    } return ent != nullptr; }
	bool setIcon(size_t index, IconEntry const &icon, const int *id = nullptr)       {       Entry *ent = getEntry(index); if (ent) { if(id) ent->id=*id; memcpy(&ent->icon, &icon, sizeof(icon)); } return ent != nullptr; }

	template <typename T>
	bool assignImage(size_t index, const T &begin, const T &end) {
		Entry *ent = getEntry(index);
		if (!ent) return false;
		ent->image.assign(begin, end);
		ent->icon.dwBytesInRes = ent->image.size();
		return true;
	}
	const IconImage* getImage(size_t index) const {
		const Entry *ent = getEntry(index);
		return ent ? &ent->image : nullptr;
	}

	bool getHotSpot(size_t index, WORD &x, WORD &y) {
		if (!isCursor()) return false;
		Entry *ent = getEntry(index);
		if (!ent) return false;
		x = ent->icon.ih.cur.wHotSpotX;
		y = ent->icon.ih.cur.wHotSpotY;
		return true;
	}
	bool setHotSpot(size_t index, WORD x, WORD y) {
		if (!isCursor()) resetHotSpot();
		Entry *ent = getEntry(index);
		if (ent) {
			ent->icon.ih.cur.wHotSpotX = x;
			ent->icon.ih.cur.wHotSpotY = y;
		}
		return ent != nullptr;
	}
	void resetHotSpot(WORD type = 2) {
		// change cursor type and clear hotspot
		dir_.idType = type;
		for (auto it = entries_.begin(); it != entries_.end(); ++it) {
			it->icon.ih.cur.wHotSpotX = 0;
			it->icon.ih.cur.wHotSpotY = 0;
		}
	}

	bool load(const BYTE *ptr, size_t len) {
		if (len < sizeof(dir_)) return false;
		const BYTE  *orig_ptr = ptr;
		const size_t orig_len = len;

		clear();
		memcpy(&dir_, ptr, sizeof(dir_));
		ptr += sizeof(dir_);
		len -= sizeof(dir_);

		const WORD type  = dir_.idType;
		const WORD count = dir_.idCount;
		const size_t entsz = sizeof(IconEntry);
		if ((dir_.idReserved != 0) ||
			(type != 1 && type != 2) || // icon or cursor
			(count <= 0) ||
			(len < entsz*count))
		{
			clear();
			return false;
		}

		for (WORD n = 0; n < count; ++n) {
			entries_.push_back(ptr);
			ptr += entsz;
			len -= entsz;
		}

		for (WORD n = 0; n < count; ++n) {
			Entry &ent(entries_[n]);
			const DWORD size   = ent.icon.dwBytesInRes;
			const DWORD offset = ent.icon.dwImageOffset;
			if (offset + size > orig_len) {
				clear();
				return false;
			}
			ent.image.assign(orig_ptr + offset, orig_ptr + offset + size);
		}
		return true;
	}

	template <typename T>
	void save(std::vector<T> &vec) {
		static_assert(sizeof (T) == 1, "sizeof(T) == 1");
		size_t total = sizeof(dir_) + sizeof(IconEntry) * entries_.size();
		size_t offset = total;
		for (auto it = entries_.cbegin(); it != entries_.cend(); ++it) {
			total += it->image.size();
		}
		vec.resize(total);
		T* ptr = &vec.front();
		T* const imgbase = ptr;

		memcpy(ptr, &dir_, sizeof(dir_));
		ptr += sizeof(dir_);

		for (auto it = entries_.cbegin(); it != entries_.cend(); ++it) {
			IconEntry ent;
			memcpy(&ent, &it->icon, sizeof(ent));
			ent.dwImageOffset = offset;
			memcpy(ptr, &ent, sizeof(ent));
			ptr += sizeof(ent);
			const size_t imgsz = it->image.size();
			if (imgsz > 0) memcpy(imgbase + offset, &it->image.front(), imgsz);
			offset += imgsz;
		}
	}

	template <class LIST>
	bool build(const LIST &list, WORD type = 1) {
		clear();
		dir_.idType = type;
		for (auto it = list.begin(); it != list.end(); ++it) {
			entries_.push_back(Entry());
			Entry *ent = &entries_.back();
			ICONHDR hdr = {0};
			if ((*it)(hdr, ent->image, &ent->id)) {
				memcpy(&ent->icon.ih, &hdr, sizeof(hdr));
				ent->icon.dwBytesInRes = ent->image.size();
				ent->icon.dwImageOffset = 0;
			} else {
				clear();
				return false;
			}
		}
		return true;
	}
};

class GroupContainer {
	ICONDIR dir_;
	typedef GRPICONDIRENTRY GrpEntry;
	std::vector<GrpEntry> entries_;

	const GrpEntry* getEntry(size_t index) const { return (index < getCount()) ? &entries_[index] : nullptr; }
	/**/  GrpEntry* getEntry(size_t index)       { return (index < getCount()) ? &entries_[index] : nullptr; }
public:
	GroupContainer() {}
	~GroupContainer() { clear(); }

	void clear() {
		::ZeroMemory(&dir_, sizeof(dir_));
		entries_.clear();
	}

	size_t getCount() const { return entries_.size(); }

	bool getID(size_t index, int &id) const { const GrpEntry *ent = getEntry(index); if (ent) id = (int)ent->nID;  return ent != nullptr; }
	bool setID(size_t index, int  id)       {       GrpEntry *ent = getEntry(index); if (ent) ent->nID = (WORD)id; return ent != nullptr; }

	bool load(const BYTE *ptr, size_t len) {
		if (len < sizeof(dir_)) return false;

		clear();
		memcpy(&dir_, ptr, sizeof(dir_));
		ptr += sizeof(dir_);
		len -= sizeof(dir_);

		const WORD type  = dir_.idType;
		const WORD count = dir_.idCount;
		const size_t entsz = sizeof(GrpEntry);
		if ((dir_.idReserved != 0) ||
			(type != 1 && type != 2) || // icon or cursor
			(count <= 0) ||
			(len < entsz*count))
		{
			clear();
			return false;
		}

		for (WORD n = 0; n < count; ++n) {
			GrpEntry ent;
			memcpy(&ent, ptr, sizeof(ent));
			entries_.push_back(ent);
			ptr += entsz;
			len -= entsz;
		}
		return true;
	}

	template <typename T>
	void save(std::vector<T> &vec) {
		static_assert(sizeof (T) == 1, "sizeof(T) == 1");
		const size_t grpsz = sizeof(GrpEntry);
		size_t total = sizeof(dir_) + grpsz * entries_.size();

		vec.resize(total);
		T* ptr = &vec.front();

		memcpy(ptr, &dir_, sizeof(dir_));
		ptr += sizeof(dir_);

		for (auto it = entries_.cbegin(); it != entries_.cend(); ++it) {
			memcpy(ptr, &(*it), grpsz);
			ptr += grpsz;
		}
	}

	bool build(const ImageContainer &image) {
		clear();
		memcpy(&dir_, &image.getDir(), sizeof(dir_));

		entries_.resize(image.getCount());
		size_t n = 0;
		int id = -1;
		ImageContainer::IconEntry icon;
		for (auto it = entries_.begin(); it != entries_.end(); ++it, ++n) {
			if (image.getIcon(n, icon, &id)) {
				memcpy(&it->ih, &icon.ih, sizeof(ICONHDR));
				it->dwBytesInRes = icon.dwBytesInRes;
				it->nID = (WORD)id;
			} else {
				clear();
				return false;
			}
		}
		return true;
	}

	template <class IMAGEGET>
	bool extract(ImageContainer &image, IMAGEGET &iget) const {
		typedef typename IMAGEGET::iterator iterator;
		image.reset(dir_, entries_.size());
		size_t n = 0;
		for (auto it = entries_.begin(); it != entries_.end(); ++it, ++n) {
			ImageContainer::IconEntry icon = {0};
			memcpy(&icon, &it->ih, sizeof(ICONHDR));
			icon.dwBytesInRes = it->dwBytesInRes;
			const int id = it->nID;
			image.setIcon(n, icon, &id);
			iterator begin, end;
			if (!iget(n, id, begin, end) || !image.assignImage(n, begin, end)) {
				image.clear();
				return false;
			}
		}
		return true;
	}

	bool extract(ImageContainer &image) const {
		image.reset(dir_, entries_.size());
		size_t n = 0;
		for (auto it = entries_.begin(); it != entries_.end(); ++it, ++n) {
			ImageContainer::IconEntry icon = {0};
			memcpy(&icon, &it->ih, sizeof(ICONHDR));
			icon.dwBytesInRes = 0;
			const int id = it->nID;
			if(!image.setIcon(n, icon, &id)) {
				image.clear();
				return false;
			}
		}
		return true;
	}
};

} // namespace IconResource 

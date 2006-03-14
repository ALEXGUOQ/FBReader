/*
 * Copyright (C) 2004-2006 Nikolay Pultsin <geometer@mawhrin.net>
 * Copyright (C) 2005 Mikhail Sobolev <mss@mawhrin.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include <iostream>

#include "ZLZipHeader.h"
#include "../ZLInputStream.h"

bool ZLZipHeader::readFrom(ZLInputStream &stream) {
	size_t startOffset = stream.offset();
	Signature = readLong(stream);
	Version = readShort(stream);
	Flags = readShort(stream);
	CompressionMethod = readShort(stream);
	ModificationTime = readShort(stream);
	ModificationDate = readShort(stream);
	CRC32 = readLong(stream);
	CompressedSize = readLong(stream);
	UncompressedSize = readLong(stream);
	NameLength = readShort(stream);
	ExtraLength = readShort(stream);
	std::cerr << "Signature = " << Signature << "\n";
	std::cerr << "startOffset = " << startOffset << "\n";
	std::cerr << "NameLength = " << NameLength << "\n";
	std::cerr << "Flags = " << Flags << "\n";
	return (Signature == 0x04034B50) && (stream.offset() == startOffset + 30) && (NameLength != 0);
}

unsigned short ZLZipHeader::readShort(ZLInputStream &stream) {
	char buffer[2];
	stream.read(buffer, 2);
	return ((((unsigned short)buffer[1]) & 0xFF) << 8) + ((unsigned short)buffer[0] & 0xFF);
}

unsigned long ZLZipHeader::readLong(ZLInputStream &stream) {
	char buffer[4];
	stream.read(buffer, 4);

	return
		((((unsigned long)buffer[3]) & 0xFF) << 24) +
		((((unsigned long)buffer[2]) & 0xFF) << 16) +
		((((unsigned long)buffer[1]) & 0xFF) << 8) +
		((unsigned long)buffer[0] & 0xFF);
}

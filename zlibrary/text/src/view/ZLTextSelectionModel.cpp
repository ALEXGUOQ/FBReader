/*
 * Copyright (C) 2004-2008 Geometer Plus <contact@geometerplus.com>
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

#include <ZLApplication.h>
#include <ZLTime.h>
#include <ZLUnicodeUtil.h>
#include <ZLibrary.h>

#include "ZLTextSelectionModel.h"
#include "ZLTextView.h"

ZLTextSelectionModel::ZLTextSelectionModel(ZLTextView &view, ZLApplication &application) : myView(view), myApplication(application), myIsActive(false), myIsEmpty(true), myDoUpdate(false), myTextIsUpToDate(true), myRangeVectorIsUpToDate(true) {
}

void ZLTextSelectionModel::setBound(Bound &bound, int x, int y) {
	if (myView.myTextElementMap.empty()) {
		return;
	}

	ZLTextElementMap::const_iterator it = myView.myTextElementMap.begin();
	for (; it != myView.myTextElementMap.end(); ++it) {
		if ((it->YStart > y) || ((it->YEnd > y) && (it->XEnd > x))) {
			break;
		}
	}

	if (it != myView.myTextElementMap.end()) {
		bound.After.ParagraphIndex = it->ParagraphIndex;
		bound.After.ElementIndex = it->ElementIndex;
		bound.After.Exists = true;
		const bool mainDir =
			it->BidiLevel % 2 == myView.myStyle.baseBidiLevel() % 2;
		bound.After.CharIndex = mainDir ?
			it->StartCharIndex :
			it->StartCharIndex + it->Length;
		if (ZLTextElementArea::RangeChecker(x, y)(*it)) {
			bound.Before.ParagraphIndex = bound.After.ParagraphIndex;
			bound.Before.ElementIndex = bound.After.ElementIndex;
			bound.Before.Exists = true;
			if (it->Kind == ZLTextElement::WORD_ELEMENT) {
				myView.myStyle.setTextStyle(it->Style, it->BidiLevel);
				ZLTextWordCursor cursor = myView.startCursor();
				cursor.moveToParagraph(it->ParagraphIndex);
				const ZLTextWord &word = (const ZLTextWord&)cursor.paragraphCursor()[it->ElementIndex];
				const int deltaX = mainDir ? x - it->XStart : it->XEnd - x;
				const int len = it->Length;
				const int start = it->StartCharIndex;
				int diff = deltaX;
				int previousDiff = diff;
				int index;
				for (index = 0; (index < len) && (diff > 0); ++index) {
					previousDiff = diff;
					diff = deltaX - myView.myStyle.wordWidth(word, start, index + 1);
				}
				if (previousDiff + diff < 0) {
					--index;
				}
				bound.After.CharIndex = start + index;
				bound.Before.CharIndex = bound.After.CharIndex;
			}
		} else if (it == myView.myTextElementMap.begin()) {
			bound.Before.Exists = false;
		} else {
			const ZLTextElementArea &previous = *(it - 1);
			bound.Before.ParagraphIndex = previous.ParagraphIndex;
			bound.Before.ElementIndex = previous.ElementIndex;
			bound.Before.CharIndex = (previous.BidiLevel % 2 == myView.myStyle.baseBidiLevel() % 2) ?
				previous.StartCharIndex + previous.Length :
				previous.StartCharIndex;
			bound.Before.Exists = true;
		}
	} else {
		const ZLTextElementArea &back = myView.myTextElementMap.back();
		bound.Before.ParagraphIndex = back.ParagraphIndex;
		bound.Before.ElementIndex = back.ElementIndex;
		bound.Before.CharIndex = back.StartCharIndex + back.Length;
		bound.Before.Exists = true;
		bound.After.Exists = false;
	}
}

void ZLTextSelectionModel::activate(int x, int y) {
	if (myView.myTextElementMap.empty()) {
		return;
	}

	myIsActive = true;
	myIsEmpty = false;
	setBound(myFirstBound, x, y);
	mySecondBound = myFirstBound;
	myCursors.clear();
	myText.erase();
	myTextIsUpToDate = true;
	myRanges.clear();
	myRangeVectorIsUpToDate = true;
}

bool ZLTextSelectionModel::BoundElement::operator == (const ZLTextSelectionModel::BoundElement &element) const {
	return
		(Exists == element.Exists) &&
		(ParagraphIndex == element.ParagraphIndex) &&
		(ElementIndex == element.ElementIndex) &&
		(CharIndex == element.CharIndex);
}

bool ZLTextSelectionModel::BoundElement::operator != (const ZLTextSelectionModel::BoundElement &element) const {
	return !operator == (element);
}

bool ZLTextSelectionModel::extendTo(int x, int y) {
	if (!myIsActive || myView.myTextElementMap.empty()) {
		return false;
	}

	Range oldRange = internalRange();
	setBound(mySecondBound, x, y);
	Range newRange = internalRange();
	myStoredX = x;
	myStoredY = y;

	if (!mySecondBound.Before.Exists) {
		startSelectionScrolling(false);
	} else if (!mySecondBound.After.Exists) {
		startSelectionScrolling(true);
	} else {
		stopSelectionScrolling();
	}

	if ((oldRange.first != newRange.first) || (oldRange.second != newRange.second)) {
		myTextIsUpToDate = false;
		myText.erase();
		myRangeVectorIsUpToDate = false;
		myRanges.clear();
		return true;
	}
	return false;
}

void ZLTextSelectionModel::deactivate() {
	stopSelectionScrolling();
	myIsActive = false;
	myDoUpdate = false;
}

void ZLTextSelectionModel::clear() {
	stopSelectionScrolling();
	myIsEmpty = true;
	myIsActive = false;
	myDoUpdate = false;
	myCursors.clear();
	myText.erase();
	myTextIsUpToDate = true;
	myRanges.clear();
	myRangeVectorIsUpToDate = true;
}

const std::vector<ZLTextSelectionModel::Range> &ZLTextSelectionModel::ranges() const {
	if (!myRangeVectorIsUpToDate && !isEmpty()) {
		Range r = internalRange();

		ZLTextWordCursor cursor = myView.startCursor();
		cursor.moveToParagraph(r.first.ParagraphIndex);
		cursor.moveToParagraphStart();
		int startLevel = 0;
		for (int i = r.first.ElementIndex; i > 0; --i) {
			switch (cursor.element().kind()) {
				case ZLTextElement::START_REVERSED_SEQUENCE_ELEMENT:
					++startLevel;
					break;
				case ZLTextElement::END_REVERSED_SEQUENCE_ELEMENT:
					--startLevel;
					break;
				default:
					break;
			}
			cursor.nextWord();
		}

		cursor.moveToParagraph(r.second.ParagraphIndex);
		cursor.moveToParagraphEnd();
		int endLevel = 0;
		for (int i = cursor.elementIndex() - r.second.ElementIndex; i > 0; --i) {
			cursor.previousWord();
			switch (cursor.element().kind()) {
				case ZLTextElement::START_REVERSED_SEQUENCE_ELEMENT:
					--endLevel;
					break;
				case ZLTextElement::END_REVERSED_SEQUENCE_ELEMENT:
					++endLevel;
					break;
				default:
					break;
			}
		}

		if ((startLevel == 0) && (endLevel == 0)) {
			myRanges.push_back(r);
		} else if (r.first.ParagraphIndex == r.second.ParagraphIndex) {
			BoundElement leftBound = r.first;
			BoundElement rightBound;
			rightBound.Exists = true;
			rightBound.ParagraphIndex = leftBound.ParagraphIndex;
			rightBound.CharIndex = 0;
			cursor.moveTo(r.first.ElementIndex, 0);
			for (int i = r.first.ElementIndex; i < r.second.ElementIndex; ++i) {
				ZLTextElement::Kind kind = cursor.element().kind();
				if ((kind == ZLTextElement::START_REVERSED_SEQUENCE_ELEMENT) ||
						(kind == ZLTextElement::END_REVERSED_SEQUENCE_ELEMENT)) {
					rightBound.ElementIndex = i;
					myRanges.push_back(Range(leftBound, rightBound));
					leftBound = rightBound;
				}
				cursor.nextWord();
			}
			myRanges.push_back(Range(leftBound, r.second));
		} else {
			BoundElement leftBound = r.first;
			if (startLevel > 0) {
				BoundElement rightBound;
				rightBound.Exists = true;
				rightBound.ParagraphIndex = leftBound.ParagraphIndex;
				rightBound.ElementIndex = leftBound.ElementIndex;
				rightBound.CharIndex = 0;
				cursor.moveToParagraph(r.first.ParagraphIndex);
				cursor.moveTo(r.first.ElementIndex, 0);
				while (startLevel > 0) {
					switch(cursor.element().kind()) {
						case ZLTextElement::START_REVERSED_SEQUENCE_ELEMENT:
							++startLevel;
							rightBound.ElementIndex = cursor.elementIndex();
							myRanges.push_back(Range(leftBound, rightBound));
							leftBound = rightBound;
							break;
						case ZLTextElement::END_REVERSED_SEQUENCE_ELEMENT:
							--startLevel;
							rightBound.ElementIndex = cursor.elementIndex();
							myRanges.push_back(Range(leftBound, rightBound));
							leftBound = rightBound;
							break;
						default:
							break;
					}
					cursor.nextWord();
				}
			}

			BoundElement rightBound1 = r.second;
			if (endLevel > 0) {
				BoundElement leftBound1;
				leftBound1.Exists = true;
				leftBound1.ParagraphIndex = rightBound1.ParagraphIndex;
				leftBound1.ElementIndex = rightBound1.ElementIndex;
				leftBound1.CharIndex = 0;
				cursor.moveToParagraph(r.second.ParagraphIndex);
				cursor.moveTo(r.second.ElementIndex, 0);
				while (endLevel > 0) {
					switch(cursor.element().kind()) {
						case ZLTextElement::START_REVERSED_SEQUENCE_ELEMENT:
							--endLevel;
							leftBound1.ElementIndex = cursor.elementIndex();
							myRanges.push_back(Range(leftBound1, rightBound1));
							rightBound1 = leftBound1;
							break;
						case ZLTextElement::END_REVERSED_SEQUENCE_ELEMENT:
							++endLevel;
							rightBound1.ElementIndex = cursor.elementIndex();
							myRanges.push_back(Range(leftBound1, rightBound1));
							rightBound1 = leftBound1;
							break;
						default:
							break;
					}
					cursor.previousWord();
				}
			}
			myRanges.push_back(Range(leftBound, rightBound1));
		}

		myRangeVectorIsUpToDate = true;
	}
	return myRanges;
}

ZLTextSelectionModel::Range ZLTextSelectionModel::internalRange() const {
	return
		(mySecondBound < myFirstBound) ?
			Range(mySecondBound.After, myFirstBound.Before) :
			Range(myFirstBound.After, mySecondBound.Before);
}

bool ZLTextSelectionModel::Bound::operator < (const Bound &bound) const {
	if (!bound.Before.Exists) {
		return false;
	}
	if (!Before.Exists) {
		return true;
	}

	if (!After.Exists) {
		return false;
	}
	if (!bound.After.Exists) {
		return true;
	}

	if (Before.ParagraphIndex < bound.Before.ParagraphIndex) {
		return true;
	}
	if (Before.ParagraphIndex > bound.Before.ParagraphIndex) {
		return false;
	}

	if (Before.ElementIndex < bound.Before.ElementIndex) {
		return true;
	}
	if (Before.ElementIndex > bound.Before.ElementIndex) {
		return false;
	}

	return Before.CharIndex < bound.Before.CharIndex;
}

bool ZLTextSelectionModel::isEmpty() const {
	if (myIsEmpty) {
		return true;
	}
	Range r = internalRange();
	return !r.first.Exists || !r.second.Exists || (r.first == r.second);
}

void ZLTextSelectionModel::startSelectionScrolling(bool forward) {
	if (mySelectionScroller.isNull()) {
		mySelectionScroller = new ZLTextSelectionScroller(*this);
	}
	ZLTextSelectionScroller::Direction direction =
		forward ?
			ZLTextSelectionScroller::SCROLL_FORWARD :
			ZLTextSelectionScroller::SCROLL_BACKWARD;
	ZLTextSelectionScroller &scroller = ((ZLTextSelectionScroller&)*mySelectionScroller);
	if (scroller.direction() != direction) {
		if (scroller.direction() != ZLTextSelectionScroller::DONT_SCROLL) {
			ZLTimeManager::instance().removeTask(mySelectionScroller);
		}
		((ZLTextSelectionScroller&)*mySelectionScroller).setDirection(direction);
		ZLTimeManager::instance().addTask(mySelectionScroller, 400);
	}
}

void ZLTextSelectionModel::stopSelectionScrolling() {
	if (!mySelectionScroller.isNull()) {
		((ZLTextSelectionScroller&)*mySelectionScroller).setDirection(ZLTextSelectionScroller::DONT_SCROLL);
		ZLTimeManager::instance().removeTask(mySelectionScroller);
	}
}

void ZLTextSelectionModel::update() {
	if (!myView.isSelectionEnabled()) {
		clear();
	} else if (myDoUpdate) {
		myDoUpdate = false;
		setBound(mySecondBound, myStoredX, myStoredY);
		myView.copySelectedTextToClipboard(ZLDialogManager::CLIPBOARD_SELECTION);
		myTextIsUpToDate = false;
		myText.erase();
		myRangeVectorIsUpToDate = false;
		myRanges.clear();
	}
}

void ZLTextSelectionModel::scrollAndExtend() {
	ZLTextSelectionScroller::Direction direction =
		((ZLTextSelectionScroller&)*mySelectionScroller).direction();
	if (direction != ZLTextSelectionScroller::DONT_SCROLL) {
		myView.scrollPage(direction == ZLTextSelectionScroller::SCROLL_FORWARD, ZLTextView::SCROLL_LINES, 1);
		myDoUpdate = true;
		myApplication.refreshWindow();
	}
}

const std::string &ZLTextSelectionModel::text() const {
	if (!myTextIsUpToDate && !isEmpty()) {
		Range r = internalRange();

		ZLTextWordCursor start = myView.startCursor();
		start.moveToParagraph(r.first.ParagraphIndex);
		start.moveTo(r.first.ElementIndex, r.first.CharIndex);

		ZLTextWordCursor end = myView.startCursor();
		end.moveToParagraph(r.second.ParagraphIndex);
		end.moveTo(r.second.ElementIndex, r.second.CharIndex);

		std::set<ZLTextParagraphCursorPtr> pcursors;
		pcursors.insert(start.paragraphCursorPtr());

		for (ZLTextWordCursor cursor = start; cursor < end; ) {
			if ((cursor < end) && cursor.isEndOfParagraph()) {
				cursor.nextParagraph();
				pcursors.insert(cursor.paragraphCursorPtr());
				myText.append(ZLibrary::EndOfLine);
				continue;
			}
			const ZLTextElement &element = cursor.element();
			switch (element.kind()) {
				case ZLTextElement::WORD_ELEMENT:
				{
					const ZLTextWord &word = (const ZLTextWord&)element;
					if (cursor.sameElementAs(end)) {
						if (start.sameElementAs(end)) {
							int skip = ZLUnicodeUtil::length(word.Data, start.charIndex());
							int length = ZLUnicodeUtil::length(word.Data, end.charIndex()) - skip;
							myText.append(word.Data + skip, length);
						} else {
							myText.append(word.Data, ZLUnicodeUtil::length(word.Data, end.charIndex()));
						}
					} else if (cursor.charIndex() == 0) {
						myText.append(word.Data, word.Size);
					} else /* cursor == start */ {
						int skip = ZLUnicodeUtil::length(word.Data, cursor.charIndex());
						myText.append(word.Data + skip, word.Size - skip);
					}
					break;
				}
				case ZLTextElement::HSPACE_ELEMENT:
				case ZLTextElement::NB_HSPACE_ELEMENT:
					myText += ' ';
					break;
				default:
					break;
			}
			cursor.nextWord();
		}

		myCursors.swap(pcursors);
		myTextIsUpToDate = true;
	}

	return myText;
}

ZLTextSelectionScroller::ZLTextSelectionScroller(ZLTextSelectionModel &selectionModel) : mySelectionModel(selectionModel), myDirection(DONT_SCROLL) {
}

void ZLTextSelectionScroller::setDirection(Direction direction) {
	myDirection = direction;
}

void ZLTextSelectionScroller::run() {
	mySelectionModel.scrollAndExtend();
}

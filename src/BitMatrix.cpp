#include "BitMatrix.h"
#include "BitArray.h"

namespace ZXing {

//void ZXing::Parse(const std::string& stringRepresentation, String setString, String unsetString) {
//	if (stringRepresentation == null) {
//		throw new IllegalArgumentException();
//	}

//	boolean[] bits = new boolean[stringRepresentation.length()];
//	int bitsPos = 0;
//	int rowStartPos = 0;
//	int rowLength = -1;
//	int nRows = 0;
//	int pos = 0;
//	while (pos < stringRepresentation.length()) {
//		if (stringRepresentation.charAt(pos) == '\n' ||
//			stringRepresentation.charAt(pos) == '\r') {
//			if (bitsPos > rowStartPos) {
//				if (rowLength == -1) {
//					rowLength = bitsPos - rowStartPos;
//				}
//				else if (bitsPos - rowStartPos != rowLength) {
//					throw new IllegalArgumentException("row lengths do not match");
//				}
//				rowStartPos = bitsPos;
//				nRows++;
//			}
//			pos++;
//		}
//		else if (stringRepresentation.substring(pos, pos + setString.length()).equals(setString)) {
//			pos += setString.length();
//			bits[bitsPos] = true;
//			bitsPos++;
//		}
//		else if (stringRepresentation.substring(pos, pos + unsetString.length()).equals(unsetString)) {
//			pos += unsetString.length();
//			bits[bitsPos] = false;
//			bitsPos++;
//		}
//		else {
//			throw new IllegalArgumentException(
//				"illegal character encountered: " + stringRepresentation.substring(pos));
//		}
//	}

//	// no EOL at end?
//	if (bitsPos > rowStartPos) {
//		if (rowLength == -1) {
//			rowLength = bitsPos - rowStartPos;
//		}
//		else if (bitsPos - rowStartPos != rowLength) {
//			throw new IllegalArgumentException("row lengths do not match");
//		}
//		nRows++;
//	}

//	BitMatrix matrix = new BitMatrix(rowLength, nRows);
//	for (int i = 0; i < bitsPos; i++) {
//		if (bits[i]) {
//			matrix.set(i % rowLength, i / rowLength);
//		}
//	}
//	return matrix;
//}

void
BitMatrix::xor(const BitMatrix& mask)
{
	if (_width != mask._width || _height != mask._height || _rowSize != mask._rowSize)
	{
		throw std::invalid_argument("BitMatrix::xor(): input matrix dimensions do not match");
	}
	
	BitArray rowArray(_width / 32 + 1);
	for (int y = 0; y < _height; y++)
	{
		int offset = y * _rowSize;
		auto& row = mask.row(y).bitArray();
		for (int x = 0; x < _rowSize; x++) {
			_bits[offset + x] ^= row[x];
		}
	}
}

void
BitMatrix::clear()
{
	std::fill(_bits.begin(), _bits.end(), 0);
}

BitArray
BitMatrix::row(int y) const
{
	BitArray row(_width);
	int offset = y * _rowSize;
	for (int x = 0; x < _rowSize; x++) {
		row.setBulk(x * 32, _bits[offset + x]);
	}
	return row;
}

void
BitMatrix::setRegion(int left, int top, int width, int height)
{
	if (top < 0 || left < 0) {
		throw std::invalid_argument("BitMatrix::setRegion(): Left and top must be nonnegative");
	}
	if (height < 1 || width < 1) {
		throw std::invalid_argument("BitMatrix::setRegion(): Height and width must be at least 1");
	}
	int right = left + width;
	int bottom = top + height;
	if (bottom > _height || right > _width) {
		throw std::invalid_argument("BitMatrix::setRegion(): The region must fit inside the matrix");
	}
	for (int y = top; y < bottom; y++) {
		int offset = y * _rowSize;
		for (int x = left; x < right; x++) {
			_bits[offset + (x / 32)] |= 1 << (x & 0x1f);
		}
	}
}

/**
* @param y row to set
* @param row {@link BitArray} to copy from
*/
void
BitMatrix::setRow(int y, BitArray row)
{
	if (row.size() != _rowSize) {
		throw std::invalid_argument("BitMatrix::setRegion(): row sizes do not match");
	}
	auto& newRow = row.bitArray();
	std::copy(newRow.begin(), newRow.end(), _bits.begin() + y *_rowSize);
}

/**
* Modifies this {@code BitMatrix} to represent the same but rotated 180 degrees
*/
void
BitMatrix::rotate180()
{
	for (int i = 0; i < (_height + 1) / 2; i++) {
		auto topRow = row(i);
		auto bottomRow = row(_height - 1 - i);
		topRow.reverse();
		bottomRow.reverse();
		setRow(i, bottomRow);
		setRow(_height - 1 - i, topRow);
	}
}

void
BitMatrix::mirror()
{
	for (int x = 0; x < _width; x++) {
		for (int y = x + 1; y < _height; y++) {
			if (get(x, y) != get(y, x)) {
				flip(y, x);
				flip(x, y);
			}
		}
	}
}

/**
* This is useful in detecting the enclosing rectangle of a 'pure' barcode.
*
* @return {@code left,top,width,height} enclosing rectangle of all 1 bits, or null if it is all white
*/
bool
BitMatrix::getEnclosingRectangle(int &left, int& top, int& width, int& height) const
{
	left = _width;
	top = _height;
	int right = -1;
	int bottom = -1;

	for (int y = 0; y < _height; y++)
	{
		for (int x32 = 0; x32 < _rowSize; x32++)
		{
			uint32_t theBits = _bits[y * _rowSize + x32];
			if (theBits != 0)
			{
				if (y < top) {
					top = y;
				}
				if (y > bottom) {
					bottom = y;
				}
				if (x32 * 32 < left) {
					int bit = 0;
					while ((theBits << (31 - bit)) == 0) {
						bit++;
					}
					if ((x32 * 32 + bit) < left) {
						left = x32 * 32 + bit;
					}
				}
				if (x32 * 32 + 31 > right) {
					int bit = 31;
					while ((theBits >> bit) == 0) {
						bit--;
					}
					if ((x32 * 32 + bit) > right) {
						right = x32 * 32 + bit;
					}
				}
			}
		}
	}
	width = right - left;
	height = bottom - top;
	return width >= 0 && height >= 0;
}

/**
* This is useful in detecting a corner of a 'pure' barcode.
*
* @return {@code x,y} coordinate of top-left-most 1 bit, or null if it is all white
*/
bool
BitMatrix::getTopLeftOnBit(int& left, int& top) const
{
	int bitsOffset = 0;
	while (bitsOffset < (int)_bits.size() && _bits[bitsOffset] == 0) {
		bitsOffset++;
	}
	if (bitsOffset == _bits.size()) {
		return false;
	}
	top = bitsOffset / _rowSize;
	left = (bitsOffset % _rowSize) * 32;

	uint32_t theBits = _bits[bitsOffset];
	int bit = 0;
	while ((theBits << (31 - bit)) == 0) {
		bit++;
	}
	left += bit;
	return true;
}

bool
BitMatrix::getBottomRightOnBit(int& right, int& bottom) const
{
	int bitsOffset = int(_bits.size()) - 1;
	while (bitsOffset >= 0 && _bits[bitsOffset] == 0) {
		bitsOffset--;
	}
	if (bitsOffset < 0) {
		return false;
	}

	bottom = bitsOffset / _rowSize;
	right = (bitsOffset % _rowSize) * 32;

	uint32_t theBits = _bits[bitsOffset];
	int bit = 31;
	while ((theBits >> bit) == 0) {
		bit--;
	}
	right += bit;
	return true;
}

} // ZXing
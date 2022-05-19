/*
*	rmx Library
*	Copyright (C) 2008-2022 by Eukaryot
*
*	Published under the GNU GPLv3 open source software license, see license.txt
*	or https://www.gnu.org/licenses/gpl-3.0.en.html
*/

#include "../rmxmedia.h"


namespace
{
	void addFontShadow(FontProcessingData& data, const FontKey& key)
	{
		// Add shadow effect
		const int oldBorderLeft   = data.mBorderLeft;
		const int oldBorderRight  = data.mBorderRight;
		const int oldBorderTop    = data.mBorderTop;
		const int oldBorderBottom = data.mBorderBottom;

		const int border = clamp(roundToInt(key.mShadowBlur * 2.0f), 0, 16);
		const int offsX = clamp(roundToInt(key.mShadowOffset.x), 0, 16);
		const int offsY = clamp(roundToInt(key.mShadowOffset.y), 0, 16);

		data.mBorderLeft   = oldBorderLeft   + std::max(0, border - offsX);
		data.mBorderRight  = oldBorderRight  + border + offsX;
		data.mBorderTop    = oldBorderTop    + std::max(0, border - offsY);
		data.mBorderBottom = oldBorderBottom + border + offsY;

		const int newWidth  = data.mBitmap.mWidth  + (data.mBorderLeft + data.mBorderRight) - (oldBorderLeft + oldBorderRight);
		const int newHeight = data.mBitmap.mHeight + (data.mBorderTop + data.mBorderBottom) - (oldBorderTop + oldBorderBottom);
		const int insetX = data.mBorderLeft - oldBorderLeft;
		const int insetY = data.mBorderTop - oldBorderTop;

		Bitmap bmp;
		bmp.create(newWidth, newHeight, 0);
		bmp.insertBlend(insetX + offsX, insetY + offsY, data.mBitmap);
		if (key.mShadowBlur > 0.0f)
		{
			bmp.gaussianBlur(bmp, key.mShadowBlur);
		}
		if (key.mShadowAlpha < 1.0f)
		{
			for (int i = 0; i < bmp.getPixelCount(); ++i)
			{
				bmp.mData[i] = roundToInt((float)(bmp.mData[i] >> 24) * key.mShadowAlpha) << 24;
			}
		}
		else
		{
			bmp.clearRGB(0);
		}
		bmp.insertBlend(insetX, insetY, data.mBitmap);
		data.mBitmap = bmp;
	}
}


FontOutput::FontOutput(const FontKey& key)
{
	mKey = key;
}

FontOutput::~FontOutput()
{
	reset();
}

void FontOutput::reset()
{
	mAtlas.clear();
	mHandleMap.clear();
}

void FontOutput::buildVertexGroups(std::vector<VertexGroup>& outVertexGroups, const std::vector<Font::TypeInfo>& infos)
{
	Texture* currentTexture = nullptr;
	SpriteAtlas::Sprite sprite;

	for (const Font::TypeInfo& info : infos)
	{
		if (nullptr == info.bitmap)
			continue;

		const uint32 character = info.unicode;
		SpriteHandleMap::iterator it2 = mHandleMap.find(character);
		if (it2 == mHandleMap.end())
		{
			if (!loadTexture(info))
				continue;
			it2 = mHandleMap.find(character);
		}
		const SpriteHandleInfo& spriteHandleInfo = it2->second;

		const bool result = mAtlas.getSprite(spriteHandleInfo.mAtlasHandle, sprite);
		RMX_ASSERT(result, "Failed to get sprite from atlas");
		if (!result)
			continue;

		if (sprite.texture != currentTexture)
		{
			currentTexture = sprite.texture;
			vectorAdd(outVertexGroups).mTexture = sprite.texture;
		}

		std::vector<Vertex>& vertices = outVertexGroups.back().mVertices;
		const size_t firstIndex = vertices.size();
		vertices.resize(firstIndex + 6);

		const float x0 = info.pos.x - (float)spriteHandleInfo.mBorderLeft;
		const float x1 = info.pos.x + (float)(info.bitmap->mWidth + spriteHandleInfo.mBorderRight);
		const float y0 = info.pos.y - (float)spriteHandleInfo.mBorderTop;
		const float y1 = info.pos.y + (float)(info.bitmap->mHeight + spriteHandleInfo.mBorderBottom);

		vertices[firstIndex + 0].mPosition.set(x0, y0);
		vertices[firstIndex + 1].mPosition.set(x0, y1);
		vertices[firstIndex + 2].mPosition.set(x1, y1);
		vertices[firstIndex + 3].mPosition.set(x1, y1);
		vertices[firstIndex + 4].mPosition.set(x1, y0);
		vertices[firstIndex + 5].mPosition.set(x0, y0);

		vertices[firstIndex + 0].mTexcoords.set(sprite.uvStart.x, sprite.uvStart.y);
		vertices[firstIndex + 1].mTexcoords.set(sprite.uvStart.x, sprite.uvEnd.y);
		vertices[firstIndex + 2].mTexcoords.set(sprite.uvEnd.x,   sprite.uvEnd.y);
		vertices[firstIndex + 3].mTexcoords.set(sprite.uvEnd.x,   sprite.uvEnd.y);
		vertices[firstIndex + 4].mTexcoords.set(sprite.uvEnd.x,   sprite.uvStart.y);
		vertices[firstIndex + 5].mTexcoords.set(sprite.uvStart.x, sprite.uvStart.y);
	}
}

void FontOutput::print(const std::vector<Font::TypeInfo>& infos)
{
	// Display with OpenGL
	if (FTX::Video->getVideoConfig().renderer != rmx::VideoConfig::Renderer::OPENGL)
		return;

#ifdef ALLOW_LEGACY_OPENGL
	// Fill vertex groups
	static std::vector<VertexGroup> vertexGroups;
	vertexGroups.clear();
	buildVertexGroups(vertexGroups, infos);

	// Render them (here still using OpenGL immediate mode rendering)
	for (const VertexGroup& vertexGroup : vertexGroups)
	{
		vertexGroup.mTexture->bind();

		glBegin(GL_TRIANGLES);
		for (const Vertex& vertex : vertexGroup.mVertices)
		{
			glTexCoord2f(vertex.mTexcoords.x, vertex.mTexcoords.y);
			glVertex2f(vertex.mPosition.x, vertex.mPosition.y);
		}
		glEnd();
	}
#else
	RMX_ASSERT(false, "Unsupported without legacy OpenGL support");
#endif
}

void FontOutput::applyToTypeInfos(std::vector<ExtendedTypeInfo>& outTypeInfos, const std::vector<Font::TypeInfo>& inTypeInfos)
{
	outTypeInfos.reserve(inTypeInfos.size());
	for (size_t i = 0; i < inTypeInfos.size(); ++i)
	{
		const Font::TypeInfo& typeInfo = inTypeInfos[i];
		if (nullptr == typeInfo.bitmap)
			continue;

		const uint32 character = typeInfo.unicode;
		SpriteHandleInfo* spriteHandleInfo = nullptr;
		{
			const SpriteHandleMap::iterator it = mHandleMap.find(character);
			if (it == mHandleMap.end())
			{
				spriteHandleInfo = &mHandleMap[character];

				FontProcessingData fontProcessingData;
				fontProcessingData.mBitmap = *typeInfo.bitmap;
				applyEffects(fontProcessingData, *spriteHandleInfo, true);
			}
			else
			{
				spriteHandleInfo = &it->second;
			}
		}

		ExtendedTypeInfo& extendedTypeInfo = vectorAdd(outTypeInfos);
		extendedTypeInfo.mCharacter = character;
		extendedTypeInfo.mBitmap = &spriteHandleInfo->mCachedBitmap;
		extendedTypeInfo.mDrawPosition = Vec2i(typeInfo.pos) - Vec2i(spriteHandleInfo->mBorderLeft, spriteHandleInfo->mBorderTop);
	}
}

void FontOutput::applyEffects(FontProcessingData& fontProcessingData, SpriteHandleInfo& info, bool cacheBitmap)
{
	// Run processor
	if (nullptr != mKey.mProcessor)
	{
		mKey.mProcessor->process(fontProcessingData);
	}

	// Optionally add shadow effect
	if (mKey.mShadowEnabled && mKey.mShadowAlpha > 0.001f)
	{
		addFontShadow(fontProcessingData, mKey);
	}

	info.mBorderLeft = fontProcessingData.mBorderLeft;
	info.mBorderRight = fontProcessingData.mBorderRight;
	info.mBorderTop = fontProcessingData.mBorderTop;
	info.mBorderBottom = fontProcessingData.mBorderBottom;

	if (cacheBitmap)
	{
		info.mCachedBitmap.swap(fontProcessingData.mBitmap);
	}
}

void FontOutput::createAtlasHandle(const Bitmap& bitmap, SpriteHandleInfo& info)
{
	FontProcessingData fontProcessingData;
	fontProcessingData.mBitmap = bitmap;
	applyEffects(fontProcessingData, info, false);

	info.mAtlasHandle = mAtlas.add(fontProcessingData.mBitmap);
}

bool FontOutput::loadTexture(const Font::TypeInfo& typeInfo)
{
	// Zeichen als Textur laden
	if (nullptr == typeInfo.bitmap)
		return false;

	const uint32 character = typeInfo.unicode;
	const SpriteHandleMap::iterator it = mHandleMap.find(character);
	if (it == mHandleMap.end())
	{
		SpriteHandleInfo& spriteHandleInfo = mHandleMap[character];
		createAtlasHandle(*typeInfo.bitmap, spriteHandleInfo);
	}
	return true;
}

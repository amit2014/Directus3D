/*
Copyright(c) 2016-2017 Panos Karabelas

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
copies of the Software, and to permit persons to whom the Software is furnished
to do so, subject to the following conditions :

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#pragma once

//= INCLUDES ========================
#include "../Core/Context.h"
#include "../FileSystem/FileSystem.h"
#include "../Core/GUIDGenerator.h"
//===================================

namespace Directus
{
	enum ResourceType
	{
		Resource_Unknown,
		Resource_Texture,
		Resource_Audio,
		Resource_Material,
		Resource_Shader,
		Resource_Mesh,
		Resource_Model,
		Resource_Cubemap,
		Resource_Script,
		Resource_Animation,
		Resource_Font
	};

	enum AsyncState
	{
		Async_Idle,
		Async_Started,
		Async_Completed,
		Async_Failed
	};

	class ENGINE_API Resource
	{
	public:
		virtual ~Resource() {}

		void RegisterResource(ResourceType resourceType)
		{
			m_resourceType = resourceType;
			m_resourceID = GENERATE_GUID;
			m_asyncState = Async_Idle;
		}

		//= PROPERTIES =========================================================================
		unsigned int GetResourceID() { return m_resourceID; }
	
		ResourceType GetResourceType() { return m_resourceType; }
		void SetResourceType(ResourceType type) { m_resourceType = type; }

		const std::string& GetResourceName() { return m_resourceName; }
		void SetResourceName(const std::string& name) { m_resourceName = name; }

		const std::string& GetResourceFilePath() { return m_resourceFilePath; }
		void SetResourceFilePath(const std::string& filePath) { m_resourceFilePath = filePath; }

		std::string GetResourceFileName();
		std::string GetResourceDirectory();
		//======================================================================================

		//= IO =====================================================
		virtual bool SaveToFile(const std::string& filePath) = 0;
		virtual bool LoadFromFile(const std::string& filePath) = 0;
		virtual void SaveToFileAsync(const std::string& filePath);
		virtual void LoadFromFileAsync(const std::string& filePath);
		virtual unsigned int GetMemoryUsageKB() { return 0; }
		//==========================================================

		AsyncState GetAsyncState() { return m_asyncState; }
		void SetAsyncState(AsyncState state) { m_asyncState = state; }

		//= HELPER FUNCTIONS ================
		template <typename T>
		static ResourceType ToResourceType();
		//===================================

	protected:	
		unsigned int m_resourceID = NOT_ASSIGNED_HASH;
		std::string m_resourceName = NOT_ASSIGNED;
		std::string m_resourceFilePath = NOT_ASSIGNED;
		ResourceType m_resourceType = Resource_Unknown;
		AsyncState m_asyncState = Async_Idle;
		Context* m_context = nullptr;
	};
}

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

//= INCLUDES ===========================
#include "Scene.h"
#include "GameObject.h"
#include "../Core/Timer.h"
#include "../Core/Context.h"
#include "../Core/Stopwatch.h"
#include "../Components/Transform.h"
#include "../Components/Camera.h"
#include "../Components/Light.h"
#include "../Components/Script.h"
#include "../Components/LineRenderer.h"
#include "../Components/Skybox.h"
#include "../Components/MeshFilter.h"
#include "../Components/MeshRenderer.h"
#include "../EventSystem/EventSystem.h"
#include "../Resource/ResourceManager.h"
#include "../Graphics/Mesh.h"
#include "../IO/FileStream.h"
#include "../FileSystem/FileSystem.h"
#include "../Logging/Log.h"
//======================================

//= NAMESPACES ================
using namespace std;
using namespace Directus::Math;
//=============================

namespace Directus
{
	Scene::Scene(Context* context) : Subsystem(context)
	{
		m_ambientLight = Vector3::Zero;
		m_fps = 0.0f;
		m_timePassed = 0.0f;
		m_frameCount = 0;
		m_progressStatus = NOT_ASSIGNED;
		m_jobsDone = 0.0f;
		m_jobsTotal = 0.0f;
		m_isLoading = false;

		SUBSCRIBE_TO_EVENT(EVENT_UPDATE, EVENT_HANDLER(Resolve));
		SUBSCRIBE_TO_EVENT(EVENT_UPDATE, EVENT_HANDLER(Update));
	}

	Scene::~Scene()
	{
		Clear();
	}

	bool Scene::Initialize()
	{
		m_mainCamera = CreateCamera();
		CreateSkybox();
		CreateDirectionalLight();
		Resolve();

		return true;
	}

	void Scene::Start()
	{
		for (const auto& gameObject : m_gameObjects)
		{
			gameObject->Start();
		}
	}

	void Scene::OnDisable()
	{
		for (const auto& gameObject : m_gameObjects)
		{
			gameObject->OnDisable();
		}
	}

	void Scene::Update()
	{
		for (const auto& gameObject : m_gameObjects)
		{
			gameObject->Update();
		}

		CalculateFPS();
	}

	void Scene::Clear()
	{
		m_gameObjects.clear();
		m_gameObjects.shrink_to_fit();

		m_renderables.clear();
		m_renderables.shrink_to_fit();

		FIRE_EVENT(EVENT_SCENE_CLEARED);
	}
	//=========================================================================================================

	//= I/O ===================================================================================================
	bool Scene::SaveToFile(const string& filePathIn)
	{
		m_progressStatus = "Saving scene...";
		Stopwatch timer;
		m_isLoading = true;

		// Add scene file extension to the filepath if it's missing
		string filePath = filePathIn;
		if (FileSystem::GetExtensionFromFilePath(filePath) != SCENE_EXTENSION)
		{
			filePath += SCENE_EXTENSION;
		}

		// Save any in-memory changes done to resources while running.
		m_context->GetSubsystem<ResourceManager>()->SaveResourcesToFiles();

		// Create a prefab file
		unique_ptr<FileStream> file = make_unique<FileStream>(filePath, FileStreamMode_Write);
		if (!file->IsOpen())
			return false;

		// Save currently loaded resource paths
		vector<string> filePaths;
		m_context->GetSubsystem<ResourceManager>()->GetResourceFilePaths(filePaths);
		file->Write(filePaths);

		//= Save GameObjects ============================
		// Only save root GameObjects as they will also save their descendants
		vector<weak_ptr<GameObject>> rootGameObjects = GetRootGameObjects();

		// 1st - GameObject count
		int rootGameObjectCount = (int)rootGameObjects.size();
		file->Write(rootGameObjectCount);

		// 2nd - GameObject IDs
		for (const auto& root : rootGameObjects)
		{
			file->Write(root.lock()->GetID());
		}

		// 3rd - GameObjects
		for (const auto& root : rootGameObjects)
		{
			root.lock()->Serialize(file.get());
		}
		//==============================================

		ClearProgressStatus();
		LOG_INFO("Scene: Saving took " + to_string((int)timer.GetElapsedTimeMs()) + " ms");
		FIRE_EVENT(EVENT_SCENE_SAVED);

		return true;
	}

	bool Scene::LoadFromFile(const string& filePath)
	{
		if (!FileSystem::FileExists(filePath))
		{
			LOG_ERROR(filePath + " was not found.");
			return false;
		}

		Clear(); 
		m_progressStatus = "Loading scene...";
		m_isLoading = true;

		// Read all the resource file paths
		unique_ptr<FileStream> file = make_unique<FileStream>(filePath, FileStreamMode_Read);
		if (!file->IsOpen())
			return false;

		Stopwatch timer;

		vector<string> resourcePaths;
		file->Read(&resourcePaths);

		m_jobsTotal = (float)resourcePaths.size();

		// Load all the resources
		auto resourceMng = m_context->GetSubsystem<ResourceManager>();
		for (const auto& resourcePath : resourcePaths)
		{
			if (FileSystem::IsEngineMeshFile(resourcePath))
			{
				resourceMng->Load<Mesh>(resourcePath);
			}

			if (FileSystem::IsEngineModelFile(resourcePath))
			{
				resourceMng->Load<Model>(resourcePath);
			}

			if (FileSystem::IsEngineMaterialFile(resourcePath))
			{
				resourceMng->Load<Material>(resourcePath);
			}

			if (FileSystem::IsEngineTextureFile(resourcePath))
			{
				resourceMng->Load<Texture>(resourcePath);
			}

			m_jobsDone++;
		}

		//= Load GameObjects ============================	
		// 1st - Root GameObject count
		int rootGameObjectCount = file->ReadInt();

		// 2nd - Root GameObject IDs
		for (int i = 0; i < rootGameObjectCount; i++)
		{
			auto gameObj = CreateGameObject().lock();
			gameObj->SetID(file->ReadInt());
		}

		// 3rd - GameObjects
		// It's important to loop with rootGameObjectCount
		// as the vector size will increase as we deserialize
		// GameObjects. This is because a GameObject will also
		// deserialize their descendants.
		for (int i = 0; i < rootGameObjectCount; i++)
		{
			m_gameObjects[i]->Deserialize(file.get(), nullptr);
		}
		//==============================================

		ClearProgressStatus();
		LOG_INFO("Scene: Loading took " + to_string((int)timer.GetElapsedTimeMs()) + " ms");
		FIRE_EVENT(EVENT_SCENE_LOADED);

		return true;
	}
	//===================================================================================================

	//= GAMEOBJECT HELPER FUNCTIONS  ====================================================================
	vector<weak_ptr<GameObject>> Scene::GetRootGameObjects()
	{
		vector<weak_ptr<GameObject>> rootGameObjects;
		for (const auto& gameObj : m_gameObjects)
		{
			if (gameObj->GetTransform()->IsRoot())
			{
				rootGameObjects.push_back(gameObj);
			}
		}

		return rootGameObjects;
	}

	weak_ptr<GameObject> Scene::GetGameObjectRoot(weak_ptr<GameObject> gameObject)
	{
		if (gameObject.expired())
			return weak_ptr<GameObject>();

		GameObject* rawPtr = gameObject.lock()->GetTransform()->GetRoot()->GetGameObject();
		return GetWeakReferenceToGameObject(rawPtr);
	}

	weak_ptr<GameObject> Scene::GetGameObjectByName(const string& name)
	{
		for (const auto& gameObject : m_gameObjects)
		{
			if (gameObject->GetName() == name)
			{
				return gameObject;
			}
		}

		return weak_ptr<GameObject>();
	}

	weak_ptr<GameObject> Scene::GetGameObjectByID(unsigned int ID)
	{
		for (const auto& gameObject : m_gameObjects)
		{
			if (gameObject->GetID() == ID)
			{
				return gameObject;
			}
		}

		return weak_ptr<GameObject>();
	}

	bool Scene::GameObjectExists(weak_ptr<GameObject> gameObject)
	{
		if (gameObject.expired())
			return false;

		return !GetGameObjectByID(gameObject.lock()->GetID()).expired() ? true : false;
	}

	// Removes a GameObject and all of it's children
	void Scene::RemoveGameObject(weak_ptr<GameObject> gameObject)
	{
		if (gameObject.expired())
			return;

		// remove any descendants
		vector<Transform*> descendants;
		gameObject.lock()->GetTransform()->GetDescendants(&descendants);
		for (const auto& descendant : descendants)
		{
			RemoveSingleGameObject(GetWeakReferenceToGameObject(descendant->GetGameObject()));
		}

		// remove this gameobject but keep it's parent
		Transform* parent = gameObject.lock()->GetTransform()->GetParent();
		RemoveSingleGameObject(gameObject);

		// if there is a parent, update it's children pool
		if (parent)
		{
			parent->ResolveChildrenRecursively();
		}
	}

	// Removes a GameObject but leaves the parent and the children as is
	void Scene::RemoveSingleGameObject(weak_ptr<GameObject> gameObject)
	{
		if (gameObject.expired())
			return;

		bool dirty = false;
		for (auto it = m_gameObjects.begin(); it < m_gameObjects.end();)
		{
			shared_ptr<GameObject> temp = *it;
			if (temp->GetID() == gameObject.lock()->GetID())
			{
				it = m_gameObjects.erase(it);
				dirty = true;
				return;
			}
			++it;
		}

		if (dirty)
		{
			Resolve();
		}
	}

	weak_ptr<GameObject> Scene::GetWeakReferenceToGameObject(GameObject* gameObject)
	{
		if (!gameObject)
			return weak_ptr<GameObject>();

		return GetGameObjectByID(gameObject->GetID());
	}

	//===================================================================================================

	//= SCENE RESOLUTION  ===============================================================================
	void Scene::Resolve()
	{
		m_renderables.clear();
		m_renderables.shrink_to_fit();

		bool hasCamera = false;
		bool hasSkybox = false;
		for (const auto& gameObject : m_gameObjects)
		{
			hasCamera = false;
			hasSkybox = false;

			// Find camera
			if (gameObject->HasComponent<Camera>())
			{
				m_mainCamera = gameObject;
				hasCamera = true;
			}

			// Find skybox
			if (gameObject->HasComponent<Skybox>())
			{
				m_skybox = gameObject;
				hasSkybox = true;
			}

			// Find renderables
			if ((gameObject->HasComponent<MeshRenderer>() && gameObject->HasComponent<MeshFilter>()) ||
				hasCamera ||
				hasSkybox ||
				gameObject->HasComponent<Light>())
			{
				m_renderables.push_back(gameObject);
			}
		}

		FIRE_EVENT_DATA(EVENT_SCENE_UPDATED, VectorToVariant(m_renderables));
	}
	//===================================================================================================

	//= TEMPORARY EXPERIMENTS  ==========================================================================
	void Scene::SetAmbientLight(float x, float y, float z)
	{
		m_ambientLight = Vector3(x, y, z);
	}

	Vector3 Scene::GetAmbientLight()
	{
		return m_ambientLight;
	}
	//======================================================================================================

	//= COMMON GAMEOBJECT CREATION =========================================================================
	weak_ptr<GameObject> Scene::CreateSkybox()
	{
		shared_ptr<GameObject> skybox = CreateGameObject().lock();
		skybox->SetName("Skybox");
		skybox->AddComponent<LineRenderer>();
		skybox->AddComponent<Skybox>();
		skybox->SetHierarchyVisibility(false);
		skybox->GetTransform()->SetParent(m_mainCamera.lock()->GetTransform());

		return skybox;
	}

	weak_ptr<GameObject> Scene::CreateCamera()
	{
		auto resourceMng = m_context->GetSubsystem<ResourceManager>();
		string scriptDirectory = resourceMng->GetStandardResourceDirectory(Resource_Script);

		shared_ptr<GameObject> camera = CreateGameObject().lock();
		camera->SetName("Camera");
		camera->AddComponent<Camera>();
		camera->GetTransform()->SetPositionLocal(Vector3(0.0f, 1.0f, -5.0f));
		camera->AddComponent<Script>().lock()->AddScript(scriptDirectory + "MouseLook.as");
		camera->AddComponent<Script>().lock()->AddScript(scriptDirectory + "FirstPersonController.as");

		return camera;
	}

	weak_ptr<GameObject> Scene::CreateDirectionalLight()
	{
		shared_ptr<GameObject> light = CreateGameObject().lock();
		light->SetName("DirectionalLight");
		light->GetTransform()->SetRotationLocal(Quaternion::FromEulerAngles(30.0f, 0.0, 0.0f));
		light->GetTransform()->SetPosition(Vector3(0.0f, 10.0f, 0.0f));

		Light* lightComp = light->AddComponent<Light>().lock().get();
		lightComp->SetLightType(LightType_Directional);
		lightComp->SetIntensity(2.0f);

		return light;
	}
	//======================================================================================================

	//= HELPER FUNCTIONS ===================================================================================
	void Scene::ClearProgressStatus()
	{
		m_progressStatus = NOT_ASSIGNED;
		m_jobsDone = 0.0f;
		m_jobsTotal = 0.0f;
		m_isLoading = false;
	}

	void Scene::CalculateFPS()
	{
		// update counters
		m_frameCount++;
		m_timePassed += m_context->GetSubsystem<Timer>()->GetDeltaTimeMs();

		if (m_timePassed >= 1000)
		{
			// calculate fps
			m_fps = (float)m_frameCount / (m_timePassed / 1000.0f);

			// reset counters
			m_frameCount = 0;
			m_timePassed = 0;
		}
	}
	//======================================================================================================
	weak_ptr<GameObject> Scene::CreateGameObject()
	{
		auto gameObj = make_shared<GameObject>(m_context);

		// First save the GameObject because the Transform (added below)
		// will call the scene to get the GameObject it's attached to
		m_gameObjects.push_back(gameObj);

		gameObj->Initialize(gameObj->AddComponent<Transform>().lock().get());

		return gameObj;
	}

	void Scene::AddGameObject(shared_ptr<GameObject> gameObject)
	{
		if (!gameObject)
			return;

		m_gameObjects.push_back(gameObject);
	}
}

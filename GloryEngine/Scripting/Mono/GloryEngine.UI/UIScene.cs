﻿using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;

namespace GloryEngine.UI
{
    /// <summary>
    /// UI scene
    /// </summary>
    public class UIScene
    {
        #region Props

        /// <summary>
        /// Renderer component that owns this scene
        /// </summary>
        public UIRenderer Renderer => _renderer;

        /// <summary>
        /// Unique ID of the UI scene
        /// </summary>
        public UInt64 ID
        {
            get => _id;
            internal set
            {
                if (_id != value || value == 0) Reset();
                _id = value;
            }
        }

        /// <summary>
        /// Number of objects that exists in this Scene
        /// </summary>
        public uint ObjectsCount
        {
            get
            {
                if (_destroyed)
                {
                    throw new Exception("This Scene has been marked for destruction.");
                }
                return UIScene_ObjectsCount(_id);
            }
        }

        #endregion

        #region Fields

        private UInt64 _id = 0;
        private Dictionary<UInt64, UIElement> _objectsCache = new Dictionary<UInt64, UIElement>();
        internal bool _destroyed = false;
        private UIRenderer _renderer;

        #endregion

        #region Constructor

        internal UIScene(UIRenderer renderer) { _renderer = renderer; }

        #endregion

        #region Methods

        /// <summary>
        /// Create a new empty object in this scene
        /// </summary>
        /// <returns>The newly created object</returns>
        public UIElement NewEmptyObject()
        {
            if (_destroyed)
            {
                throw new Exception("This Scene has been marked for destruction.");
            }
            UInt64 newObjectID = UIScene_NewEmptyObject(_id);
            UIElement sceneObject = new UIElement(newObjectID, this);
            _objectsCache.Add(newObjectID, sceneObject);
            return sceneObject;
        }

        /// <summary>
        /// Create a new empty object with name in this scene
        /// </summary>
        /// <param name="name">Name to give to the object</param>
        /// <returns>The newly created object</returns>
        public UIElement NewEmptyObject(string name)
        {
            if (_destroyed)
            {
                throw new Exception("This Scene has been marked for destruction.");
            }
            UInt64 newObjectID = UIScene_NewEmptyObjectWithName(_id, name);
            UIElement sceneObject = new UIElement(newObjectID, this);
            _objectsCache.Add(newObjectID, sceneObject);
            return sceneObject;
        }

        /// <summary>
        /// Get an object in the scene by ID
        /// </summary>
        /// <param name="objectID">ID of the object to get</param>
        /// <returns></returns>
        public UIElement GetUIElement(UInt64 objectID)
        {
            if (_destroyed)
            {
                throw new Exception("This Scene has been marked for destruction.");
            }
            if (objectID == 0) return null;
            if (_objectsCache.ContainsKey(objectID)) return _objectsCache[objectID];
            UIElement sceneObject = new UIElement(objectID, this);
            _objectsCache.Add(objectID, sceneObject);
            return sceneObject;
        }

        /// <summary>
        /// Find an element in the root of the scene with a name
        /// </summary>
        /// <param name="name">Name of the element to find</param>
        /// <returns>The first element which name matches the one supplied or null if none found</returns>
        public UIElement FindUIElement(string name)
        {
            UInt64 objectID = UIScene_FindElement(_id, name);
            if (objectID == 0) return null;
            if (_objectsCache.ContainsKey(objectID)) return _objectsCache[objectID];
            UIElement sceneObject = new UIElement(objectID, this);
            _objectsCache.Add(objectID, sceneObject);
            return sceneObject;
        }

        /// <summary>
        /// Instantiate a document into this scene
        /// </summary>
        /// <param name="document">Document to instantiate</param>
        /// <param name="parent">Element to parent the instantiated elements to</param>
        /// <returns>First UI element created by this operation</returns>
        public UIElement Instantiate(UIDocument document, UIElement parent)
        {
            if (document == null) return null;
            UInt64 elementID = UIScene_Instantiate(_id, document.ID, parent != null ? parent.ID : 0);
            if (_objectsCache.ContainsKey(elementID)) return _objectsCache[elementID];
            UIElement sceneObject = new UIElement(elementID, this);
            _objectsCache.Add(elementID, sceneObject);
            return sceneObject;
        }

        internal void OnSceneDestroy()
        {
            if (_destroyed) return;
            _destroyed = true;
            foreach (UIElement sceneObject in _objectsCache.Values)
            {
                sceneObject.OnObjectDestroy();
            }
            _objectsCache.Clear();
        }

        internal void OnSceneObjectDestroy(UInt64 objectID)
        {
            if (!_objectsCache.ContainsKey(objectID)) return;
            UIElement sceneObject = _objectsCache[objectID];
            sceneObject.OnObjectDestroy();
            _objectsCache.Remove(objectID);
        }

        internal void Reset()
        {

        }

        #endregion

        #region API Methods

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern UInt64 UIScene_NewEmptyObject(UInt64 sceneID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern UInt64 UIScene_NewEmptyObjectWithName(UInt64 sceneID, string name);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern uint UIScene_ObjectsCount(UInt64 sceneID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern UInt64 UIScene_FindElement(UInt64 sceneID, string name);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern UInt64 UIScene_Instantiate(UInt64 sceneID, UInt64 documentID, UInt64 parentID);

        #endregion
    }
}

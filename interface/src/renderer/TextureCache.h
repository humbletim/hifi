//
//  TextureCache.h
//  interface
//
//  Created by Andrzej Kapolka on 8/6/13.
//  Copyright (c) 2013 High Fidelity, Inc. All rights reserved.
//

#ifndef __interface__TextureCache__
#define __interface__TextureCache__

#include <QHash>
#include <QImage>
#include <QMap>
#include <QObject>
#include <QSharedPointer>
#include <QWeakPointer>

#include "InterfaceConfig.h"

class QOpenGLFramebufferObject;

/// Stored cached textures, including render-to-texture targets.
class TextureCache : public QObject {
public:
    
    TextureCache();
    ~TextureCache();
    
    /// Returns the ID of the permutation/normal texture used for Perlin noise shader programs.  This texture
    /// has two lines: the first, a set of random numbers in [0, 255] to be used as permutation offsets, and
    /// the second, a set of random unit vectors to be used as noise gradients.
    GLuint getPermutationNormalTextureID();

    /// Returns the ID of a texture containing the contents of the specified file, loading it if necessary. 
    GLuint getFileTextureID(const QString& filename);

    /// Returns a pointer to the primary framebuffer object.  This render target includes a depth component, and is
    /// used for scene rendering.
    QOpenGLFramebufferObject* getPrimaryFramebufferObject();
    
    /// Returns the ID of the primary framebuffer object's depth texture.  This contains the Z buffer used in rendering.
    GLuint getPrimaryDepthTextureID();
    
    /// Returns a pointer to the secondary framebuffer object, used as an additional render target when performing full
    /// screen effects.
    QOpenGLFramebufferObject* getSecondaryFramebufferObject();
    
    /// Returns a pointer to the tertiary framebuffer object, used as an additional render target when performing full
    /// screen effects.
    QOpenGLFramebufferObject* getTertiaryFramebufferObject();
    
    virtual bool eventFilter(QObject* watched, QEvent* event);

private:
    
    QOpenGLFramebufferObject* createFramebufferObject();
    
    GLuint _permutationNormalTextureID;

    QHash<QString, GLuint> _fileTextureIDs;

    GLuint _primaryDepthTextureID;
    QOpenGLFramebufferObject* _primaryFramebufferObject;
    QOpenGLFramebufferObject* _secondaryFramebufferObject;
    QOpenGLFramebufferObject* _tertiaryFramebufferObject;
};

/// A simple object wrapper for an OpenGL texture.
class Texture {
public:
    
    Texture();
    ~Texture();

    GLuint getID() const { return _id; }

private:
    
    GLuint _id;
};

/// Caches textures according to pupillary dilation.
class DilatedTextureCache {
public:

    DilatedTextureCache(const QString& filename, int innerRadius, int outerRadius);

    /// Returns a pointer to a texture with the requested amount of dilation.
    QSharedPointer<Texture> getTexture(float dilation);
    
private:

    QImage _image;
    int _innerRadius;
    int _outerRadius;
    
    QMap<float, QWeakPointer<Texture> > _textures;
};

#endif /* defined(__interface__TextureCache__) */

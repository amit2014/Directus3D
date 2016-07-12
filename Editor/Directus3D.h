/*
Copyright(c) 2016 Panos Karabelas

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

//= INCLUDES ===========
#include <QObject>
#include <QWidget>
#include <QPaintEngine>
#include <QResizeEvent>
#include "Core/Engine.h"
#include "Core/Socket.h"
//======================

class Directus3D : public QWidget
{
	Q_OBJECT
    Q_DISABLE_COPY(Directus3D)

public:
    Directus3D(QWidget* parent = NULL);
    virtual ~Directus3D();
	Socket* GetEngineSocket();

    // I will take care of the drawing
	virtual QPaintEngine* paintEngine() const { return NULL; }
protected:
	virtual void resizeEvent(QResizeEvent* evt);
	virtual void paintEvent(QPaintEvent* evt);

private:
	void InitializeEngine();
	void ShutdownEngine();
	void Resize(int width, int height);

	Socket* m_socket;
	Engine* m_engine;

public slots:
    void Update();
};
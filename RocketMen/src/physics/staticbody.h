
#pragma once

#include <includes.h>

class StaticbodyData;

class Staticbody
{
public:
	Staticbody(const Vector2& position);
	~Staticbody();

	StaticbodyData* getImpl() const;
private:
	StaticbodyData* m_impl;
};
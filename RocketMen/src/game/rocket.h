
#pragma once

#include <core/entity.h>
#include <core/entity_factory.h>

namespace rm
{
	class Rocket : public Entity
	{
	public:
		DECLARE_ENTITY(EntityType::Rocket);

	public:
		Rocket();
		Rocket(Vector2 direction, float accelerationPower);
		virtual ~Rocket();

		void initialize(Entity* owner, Vector2 direction, float power);

		virtual void update(float deltaTime)      override;
		virtual void fixedUpdate(float deltaTime) override;

		void setAccelerationPower(float accelerationPower);
		float getAccelerationPower() const;
		Rigidbody* getRigidbody() const;

		/** Serialize whole object */
		template<typename Stream>
		bool serializeFull(Stream& stream);

		/** Serialize commonly updated properties */
		template<typename Stream>
		bool serialize(Stream& stream);

		/** Serialize client-owned variables */
		template<typename Stream>
		bool reverseSerialize(Stream& stream);

		virtual void startContact(Entity* other) override;
		virtual void endContact(Entity* other)   override;

	private:
		virtual void setupRigidBody();
		bool       m_isInitialized;
		Rigidbody* m_rigidbody;
		Entity*    m_owner;
		Vector2    m_direction;
		float      m_accelerationPower;
		float      m_lifetimeSeconds;
		bool       m_gracePeriod;
	};

};
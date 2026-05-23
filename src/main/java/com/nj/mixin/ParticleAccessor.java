package com.nj.mixin;

import net.minecraft.client.particle.Particle;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.gen.Accessor;

@Mixin(Particle.class)
public interface ParticleAccessor {

    @Accessor("lastX")
    double getLastX();
    @Accessor("lastX")
    void setLastX(double val);

    @Accessor("lastY")
    double getLastY();
    @Accessor("lastY")
    void setLastY(double val);

    @Accessor("lastZ")
    double getLastZ();
    @Accessor("lastZ")
    void setLastZ(double val);

    @Accessor("x")
    double getX();
    @Accessor("x")
    void setX(double val);

    @Accessor("y")
    double getY();
    @Accessor("y")
    void setY(double val);

    @Accessor("z")
    double getZ();
    @Accessor("z")
    void setZ(double val);

    @Accessor("velocityX")
    double getVelocityX();
    @Accessor("velocityX")
    void setVelocityX(double val);

    @Accessor("velocityY")
    double getVelocityY();
    @Accessor("velocityY")
    void setVelocityY(double val);

    @Accessor("velocityZ")
    double getVelocityZ();
    @Accessor("velocityZ")
    void setVelocityZ(double val);

    @Accessor("gravityStrength")
    float getGravityStrength();

    @Accessor("velocityMultiplier")
    float getVelocityMultiplier();

    @Accessor("age")
    int getAge();
    @Accessor("age")
    void setAge(int val);

    @Accessor("dead")
    boolean isDead();
    @Accessor("dead")
    void setDead(boolean val);
}
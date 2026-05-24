package com.nj.mixin;

import com.nj.render.RenderNative;
import net.minecraft.client.render.Camera;
import net.minecraft.client.render.Frustum;
import net.minecraft.client.render.RenderTickCounter;
import net.minecraft.client.render.WorldRenderer;
import net.minecraft.client.render.state.WorldRenderState;
import net.minecraft.client.world.ClientWorld;
import net.minecraft.entity.Entity;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.Shadow;
import org.spongepowered.asm.mixin.Unique;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.Redirect;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;

@Mixin(WorldRenderer.class)
public abstract class WorldRendererMixin {

    @Unique
    private static final int BATCH_THRESHOLD = 16;

    @Shadow
    private ClientWorld world;

    @Unique
    private boolean nightjava$active;

    @Unique
    private boolean[] nightjava$visible;

    @Unique
    private List<Entity> nightjava$entityList;

    @Inject(method = "fillEntityRenderStates", at = @At("HEAD"))
    private void precomputeVisibility(Camera camera, Frustum frustum, RenderTickCounter tickCounter, WorldRenderState renderStates, CallbackInfo ci) {
        if (!RenderNative.isAvailable()) return;
        if (world == null) return;

        List<Entity> entityList = new ArrayList<>();
        for (Entity e : world.getEntities()) {
            entityList.add(e);
        }
        if (entityList.size() < BATCH_THRESHOLD) return;

        nightjava$entityList = entityList;
        nightjava$visible = RenderNative.computeVisibility(entityList, frustum, camera);
        nightjava$active = true;
    }

    @Inject(method = "fillEntityRenderStates", at = @At("RETURN"))
    private void clearVisibility(CallbackInfo ci) {
        nightjava$active = false;
        nightjava$visible = null;
        nightjava$entityList = null;
    }

    @Redirect(
        method = "fillEntityRenderStates",
        at = @At(value = "INVOKE", target = "Lnet/minecraft/client/world/ClientWorld;getEntities()Ljava/lang/Iterable;")
    )
    private Iterable<Entity> redirectGetEntities(ClientWorld instance) {
        if (!nightjava$active || nightjava$visible == null || nightjava$entityList == null) {
            return world.getEntities();
        }

        return () -> new Iterator<Entity>() {
            final Iterator<Entity> it = nightjava$entityList.iterator();
            int idx;
            Entity cached;

            void advance() {
                while (cached == null && it.hasNext()) {
                    Entity e = it.next();
                    if (idx < nightjava$visible.length && nightjava$visible[idx]) {
                        cached = e;
                    }
                    idx++;
                }
            }

            @Override
            public boolean hasNext() {
                advance();
                return cached != null;
            }

            @Override
            public Entity next() {
                advance();
                Entity e = cached;
                cached = null;
                return e;
            }
        };
    }
}
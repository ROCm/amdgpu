/* SPDX-License-Identifier: MIT */
#ifndef AMDKCL_DRM_ATOMIC_COMMIT_H
#define AMDKCL_DRM_ATOMIC_COMMIT_H

#include <drm/drm_atomic.h>

/*
 * drm: Rename struct drm_atomic_state to drm_atomic_commit
 *
 * On older kernels, struct drm_atomic_commit does not exist.
 * Provide compat defines so code using the new name compiles on old kernels.
 */
#ifndef HAVE_STRUCT_DRM_ATOMIC_COMMIT
#define drm_atomic_commit drm_atomic_state
#define drm_atomic_commit_alloc drm_atomic_state_alloc
#define drm_atomic_commit_put drm_atomic_state_put
#define drm_atomic_commit_clear drm_atomic_state_clear
#define drm_atomic_commit_default_clear drm_atomic_state_default_clear
#define drm_atomic_commit_default_release drm_atomic_state_default_release
#endif

#endif /* AMDKCL_DRM_ATOMIC_COMMIT_H */

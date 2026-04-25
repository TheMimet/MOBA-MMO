export interface WorldPosition {
  x: number;
  y: number;
  z: number;
}

export const ARENA_MIN_BOUNDS: WorldPosition = {
  x: -3500,
  y: -2500,
  z: -500,
};

export const ARENA_MAX_BOUNDS: WorldPosition = {
  x: 3500,
  y: 2500,
  z: 1500,
};

export const ARENA_SAFE_RETURN_POSITION: WorldPosition = {
  x: 0,
  y: 0,
  z: 220,
};

function clamp(value: number, min: number, max: number): number {
  return Math.min(Math.max(value, min), max);
}

export function isInsideArenaBounds(position: WorldPosition): boolean {
  return (
    Number.isFinite(position.x) &&
    Number.isFinite(position.y) &&
    Number.isFinite(position.z) &&
    position.x >= ARENA_MIN_BOUNDS.x &&
    position.x <= ARENA_MAX_BOUNDS.x &&
    position.y >= ARENA_MIN_BOUNDS.y &&
    position.y <= ARENA_MAX_BOUNDS.y &&
    position.z >= ARENA_MIN_BOUNDS.z &&
    position.z <= ARENA_MAX_BOUNDS.z
  );
}

export function clampToArenaBounds(position: WorldPosition): WorldPosition {
  if (!Number.isFinite(position.x) || !Number.isFinite(position.y) || !Number.isFinite(position.z)) {
    return { ...ARENA_SAFE_RETURN_POSITION };
  }

  if (position.z < ARENA_MIN_BOUNDS.z) {
    return { ...ARENA_SAFE_RETURN_POSITION };
  }

  return {
    x: clamp(position.x, ARENA_MIN_BOUNDS.x, ARENA_MAX_BOUNDS.x),
    y: clamp(position.y, ARENA_MIN_BOUNDS.y, ARENA_MAX_BOUNDS.y),
    z: clamp(position.z, ARENA_MIN_BOUNDS.z, ARENA_MAX_BOUNDS.z),
  };
}

export function positionsMatch(left: WorldPosition, right: WorldPosition): boolean {
  return left.x === right.x && left.y === right.y && left.z === right.z;
}

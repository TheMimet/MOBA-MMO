import type {
  Character,
  CharacterStats,
  InventoryItem,
  QuestState,
} from "../../../generated/prisma/index.js";

import { clampToArenaBounds } from "../arena/bounds.js";

interface CharacterSnapshotRecord extends Character {
  stats?: CharacterStats | null;
  inventory?: InventoryItem[];
  questStates?: QuestState[];
}

export interface CharacterStateResponse {
  character: {
    id: string;
    name: string;
    classId: string;
    level: number;
    experience: number;
    position: {
      x: number;
      y: number;
      z: number;
    };
    appearance: {
      presetId: number;
      colorIndex: number;
      shade: number;
      transparent: number;
      textureDetail: number;
    };
  };
  inventory: InventoryItem[];
  questStates: QuestState[];
  stats: CharacterStats | null;
}

export function serializeCharacterState(character: CharacterSnapshotRecord): CharacterStateResponse {
  const safePosition = clampToArenaBounds({
    x: character.positionX,
    y: character.positionY,
    z: character.positionZ,
  });

  return {
    character: {
      id: character.id,
      name: character.name,
      classId: character.classId,
      level: character.level,
      experience: character.experience,
      position: {
        x: safePosition.x,
        y: safePosition.y,
        z: safePosition.z,
      },
      appearance: {
        presetId: character.presetId,
        colorIndex: character.colorIndex,
        shade: character.shade,
        transparent: character.transparent,
        textureDetail: character.textureDetail,
      },
    },
    inventory: character.inventory ?? [],
    questStates: character.questStates ?? [],
    stats: character.stats ?? null,
  };
}

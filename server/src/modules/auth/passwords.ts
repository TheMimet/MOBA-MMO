import { randomBytes, scrypt, timingSafeEqual } from "node:crypto";
import { promisify } from "node:util";

const scryptAsync = promisify(scrypt);
const PASSWORD_HASH_VERSION = "scrypt-v1";
const KEY_LENGTH = 64;

interface PasswordHashParts {
  version: string;
  salt: string;
  hash: string;
}

function parsePasswordHash(value: string): PasswordHashParts | null {
  const parts = value.split("$");
  if (parts.length !== 3) {
    return null;
  }

  const [version, salt, hash] = parts as [string, string, string];
  return version && salt && hash ? { version, salt, hash } : null;
}

export function normalizeUsername(value: string | undefined): string {
  return value?.trim() ?? "";
}

export function isValidUsername(username: string): boolean {
  return /^[a-zA-Z0-9_.-]{3,32}$/.test(username);
}

export function isValidPassword(password: string): boolean {
  return password.length >= 6 && password.length <= 128;
}

export async function hashPassword(password: string): Promise<string> {
  const salt = randomBytes(16).toString("base64url");
  const hash = (await scryptAsync(password, salt, KEY_LENGTH)) as Buffer;
  return `${PASSWORD_HASH_VERSION}$${salt}$${hash.toString("base64url")}`;
}

export async function verifyPassword(password: string, passwordHash: string): Promise<boolean> {
  const parsed = parsePasswordHash(passwordHash);
  if (!parsed || parsed.version !== PASSWORD_HASH_VERSION) {
    return false;
  }

  const expectedHash = Buffer.from(parsed.hash, "base64url");
  const actualHash = (await scryptAsync(password, parsed.salt, expectedHash.length)) as Buffer;
  return expectedHash.length === actualHash.length && timingSafeEqual(expectedHash, actualHash);
}

-- Arena replay module tables
CREATE TABLE IF NOT EXISTS `character_arena_replays` (
  `id` INT UNSIGNED NOT NULL AUTO_INCREMENT,
  `arenaTypeId` INT NULL DEFAULT NULL,
  `typeId` INT NULL DEFAULT NULL,
  `contentSize` INT NULL DEFAULT NULL,
  `contents` LONGBLOB NULL,
  `mapId` INT NULL DEFAULT NULL,
  `winnerTeamName` VARCHAR(255) NULL DEFAULT NULL,
  `winnerTeamRating` INT UNSIGNED NULL DEFAULT 0,
  `winnerTeamMMR` INT UNSIGNED NULL DEFAULT 0,
  `loserTeamName` VARCHAR(255) NULL DEFAULT NULL,
  `loserTeamRating` INT UNSIGNED NULL DEFAULT 0,
  `loserTeamMMR` INT UNSIGNED NULL DEFAULT 0,
  `winnerPlayerGuids` TEXT NULL DEFAULT NULL,
  `loserPlayerGuids` TEXT NULL DEFAULT NULL,
  `timesWatched` INT UNSIGNED NOT NULL DEFAULT 0,
  `savedBy` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NULL DEFAULT '0',
  `timestamp` TIMESTAMP NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `character_saved_replays` (
  `id` INT UNSIGNED NOT NULL AUTO_INCREMENT,
  `character_id` INT UNSIGNED NOT NULL,
  `replay_id` INT UNSIGNED NOT NULL,
  `created_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`),
  UNIQUE KEY `uniq_character_replay` (`character_id`, `replay_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `character_bg_replays` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `arenaTypeId` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `typeId` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `contentSize` int(10) unsigned NOT NULL DEFAULT '0',
  `contents` longblob NOT NULL,
  `mapId` int(10) unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

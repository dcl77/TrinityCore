-- Add rated column to pvpstats_battlegrounds
ALTER TABLE `pvpstats_battlegrounds` ADD COLUMN `rated` tinyint(3) UNSIGNED NOT NULL DEFAULT 0 AFTER `type`;

-- Change attr_1 and attr_2 in pvpstats_players to signed mediumint to support rating changes
ALTER TABLE `pvpstats_players` MODIFY COLUMN `attr_1` mediumint(9) NOT NULL DEFAULT 0;
ALTER TABLE `pvpstats_players` MODIFY COLUMN `attr_2` mediumint(9) NOT NULL DEFAULT 0;

-- Create pvpstats_arena_teams table
DROP TABLE IF EXISTS `pvpstats_arena_teams`;
CREATE TABLE `pvpstats_arena_teams` (
  `battleground_id` bigint(20) UNSIGNED NOT NULL,
  `arena_team_id` int(10) UNSIGNED NOT NULL,
  `winner` tinyint(3) UNSIGNED NOT NULL,
  `rating_change` int(11) NOT NULL,
  `mmr` int(10) UNSIGNED NOT NULL,
  PRIMARY KEY (`battleground_id`,`arena_team_id`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

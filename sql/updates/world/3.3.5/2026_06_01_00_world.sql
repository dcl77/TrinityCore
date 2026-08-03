-- Assign script name to Jaina and Sylvanas Part 1 in Pit of Saron
UPDATE `creature_template` SET `ScriptName` = 'npc_jaina_sylvanas_pos_intro' WHERE `entry` IN (36990, 36993);

-- Dialogue for Scourgelord Tyrannus (NPC_TYRANNUS_EVENTS = 36794)
DELETE FROM `creature_text` WHERE `entry` = 36794 AND `groupid` IN (4, 5, 6, 7);
INSERT INTO `creature_text` (`entry`, `groupid`, `id`, `text`, `type`, `language`, `probability`, `emote`, `duration`, `sound`, `BroadcastTextId`, `TextRange`, `comment`) VALUES
(36794, 4, 0, 'Intruders have entered the master\'s domain. Signal the alarms!', 14, 0, 100, 0, 0, 16747, 37093, 0, 'Scourgelord Tyrannus - SAY_INTRO_1'),
(36794, 5, 0, 'Hrmph, fodder. Not even fit to labor in the quarry. Relish these final moments for soon you will be nothing more than mindless undead.', 14, 0, 100, 0, 0, 16748, 37094, 0, 'Scourgelord Tyrannus - SAY_INTRO_2'),
(36794, 6, 0, 'Your last waking memory will be of agonizing pain.', 14, 0, 100, 0, 0, 16749, 37095, 0, 'Scourgelord Tyrannus - SAY_INTRO_3'),
(36794, 7, 0, 'Minions, destroy these interlopers!', 14, 0, 100, 0, 0, 16751, 37096, 0, 'Scourgelord Tyrannus - SAY_INTRO_4');

-- Dialogue for Jaina (NPC_JAINA_PART1 = 36993)
DELETE FROM `creature_text` WHERE `entry` = 36993 AND `groupid` IN (6, 7, 8, 9, 10);
INSERT INTO `creature_text` (`entry`, `groupid`, `id`, `text`, `type`, `language`, `probability`, `emote`, `duration`, `sound`, `BroadcastTextId`, `TextRange`, `comment`) VALUES
(36993, 6, 0, 'Heroes of the Alliance, attack!', 14, 0, 100, 0, 0, 16626, 37087, 0, 'Lady Jaina Proudmoore - SAY_INTRO_1'),
(36993, 7, 0, 'NO! YOU MONSTER!', 14, 0, 100, 0, 0, 16627, 37088, 0, 'Lady Jaina Proudmoore - SAY_INTRO_2'),
(36993, 8, 0, 'I do what I must. Please forgive me, noble soldiers.', 14, 0, 100, 0, 0, 16628, 37089, 0, 'Lady Jaina Proudmoore - SAY_INTRO_3'),
(36993, 9, 0, 'You will have to make your way across this quarry on your own.', 12, 0, 100, 0, 0, 16629, 37090, 0, 'Lady Jaina Proudmoore - SAY_INTRO_4'),
(36993, 10, 0, 'Free any Alliance slaves that you come across. We will most certainly need their assistance in battling Tyrannus. I will gather reinforcements and join you on the other side of the quarry.', 12, 0, 100, 0, 0, 16630, 37091, 0, 'Lady Jaina Proudmoore - SAY_INTRO_5');

-- Dialogue for Sylvanas (NPC_SYLVANAS_PART1 = 36990)
DELETE FROM `creature_text` WHERE `entry` = 36990 AND `groupid` IN (5, 6, 7, 8);
INSERT INTO `creature_text` (`entry`, `groupid`, `id`, `text`, `type`, `language`, `probability`, `emote`, `duration`, `sound`, `BroadcastTextId`, `TextRange`, `comment`) VALUES
(36990, 5, 0, 'Soldiers of the Horde, attack!', 14, 0, 100, 0, 0, 17045, 37392, 0, 'Lady Sylvanas Windrunner - SAY_INTRO_1'),
(36990, 6, 0, 'Pathetic weaklings.', 14, 0, 100, 0, 0, 17046, 37393, 0, 'Lady Sylvanas Windrunner - SAY_INTRO_2'),
(36990, 7, 0, 'You will have to battle your way through this cesspit on your own.', 12, 0, 100, 0, 0, 17047, 37394, 0, 'Lady Sylvanas Windrunner - SAY_INTRO_3'),
(36990, 8, 0, 'Free any Horde slaves that you come across. We will most certainly need their assistance in battling Tyrannus. I will gather reinforcements and join you on the other side of the quarry.', 12, 0, 100, 0, 0, 17048, 37395, 0, 'Lady Sylvanas Windrunner - SAY_INTRO_4');

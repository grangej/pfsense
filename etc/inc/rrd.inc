<?php
/* $Id$ */
/*
  Copyright (C) 2010 Seth Mos <seth.mos@dds.nl>
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
  this list of conditions and the following disclaimer.

  2. Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

  THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
  INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
  AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
  AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
  OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.

  */

/*
	pfSense_BUILDER_BINARIES:	/bin/rm	/usr/bin/nice	/usr/local/bin/rrdtool	/bin/cd
	pfSense_MODULE:	rrd
*/

/* include all configuration functions */

function dump_rrd_to_xml($rrddatabase, $xmldumpfile) {
	$rrdtool = "/usr/bin/nice -n20 /usr/local/bin/rrdtool";
	if(file_exists($xmldumpfile))
		mwexec("rm {$xmldumpfile}");

	exec("$rrdtool dump {$rrddatabase} {$xmldumpfile} 2>&1", $dumpout, $dumpret);
	if ($dumpret <> 0) {
		$dumpout = implode(" ", $dumpout);
		log_error(sprintf(gettext('RRD dump failed exited with %1$s, the error is: %2$s'), $dumpret, $dumpout));
	}
	return($dumpret);
}

function restore_rrd() {
	global $g;

	$rrddbpath = "/var/db/rrd/";
	$rrdtool = "/usr/bin/nice -n20 /usr/local/bin/rrdtool";

	$rrdrestore = "";
	$rrdreturn = "";
	if (file_exists("{$g['cf_conf_path']}/rrd.tgz")) {
		foreach (glob("{$rrddbpath}/*.xml") as $xml_file) {
			unlink($xml_file);
		}
		exec("cd /;LANG=C /usr/bin/tar -xzf {$g['cf_conf_path']}/rrd.tgz 2>&1", $rrdrestore, $rrdreturn);
		$rrdrestore = implode(" ", $rrdrestore);
		if($rrdreturn != 0) {
			log_error("RRD restore failed exited with $rrdreturn, the error is: $rrdrestore\n");
		}
		foreach (glob("{$rrddbpath}/*.xml") as $xml_file) {
			$rrd_file = preg_replace('/\.xml$/', ".rrd", $xml_file);
			if (file_exists("{$rrd_file}")) {
				unlink($rrd_file);
			}
			$output = array();
			$status = null;
			exec("$rrdtool restore -f '{$xml_file}' '{$rrd_file}'", $output, $status);
			if ($status) {
				log_error("rrdtool restore -f '{$xml_file}' '{$rrd_file}' failed returning {$status}.");
				continue;
			}
			unlink($xml_file);
		}
		return true;
	}
	return false;
}

function create_new_rrd($rrdcreatecmd) {
	$rrdcreateoutput = array();
	$rrdcreatereturn = 0;
	exec("$rrdcreatecmd 2>&1", $rrdcreateoutput, $rrdcreatereturn);
	if ($rrdcreatereturn <> 0) {
		$rrdcreateoutput = implode(" ", $rrdcreateoutput);
		log_error(sprintf(gettext('RRD create failed exited with %1$s, the error is: %2$s'), $rrdcreatereturn, $rrdcreateoutput));
	}
	return $rrdcreatereturn;
}

function migrate_rrd_format($rrdoldxml, $rrdnewxml) {
	if(!file_exists("/tmp/rrd_notice_sent.txt")) {
		exec("echo 'Converting RRD configuration to new format.  This might take a bit...' | wall");
		touch("/tmp/rrd_notice_sent.txt");
	}
	$numrraold = count($rrdoldxml['rra']);
	$numdsold = count($rrdoldxml['ds']);
	$numrranew = count($rrdnewxml['rra']);
	$numdsnew = count($rrdnewxml['ds']);
	log_error(sprintf(gettext('Import RRD has %1$s DS values and %2$s RRA databases, new format RRD has %3$s DS values and %4$s RRA databases'), $numdsold, $numrraold, $numdsnew ,$numrranew));
	
	/* add data sources not found in the old array from the new array */
	$i = 0;
	foreach($rrdnewxml['ds'] as $ds) {
		if(!is_array($rrdoldxml['ds'][$i])) {
			$rrdoldxml['ds'][$i] = $rrdnewxml['ds'][$i];
			/* set unknown values to 0 */
			$rrdoldxml['ds'][$i]['last_ds'] = " 0.0000000000e+00 ";
			$rrdoldxml['ds'][$i]['value'] = " 0.0000000000e+00 ";
			$rrdoldxml['ds'][$i]['unknown_sec'] = "0";
		}
		$i++;
	}

	$i = 0;
	$rracountold = count($rrdoldxml['rra']);
	$rracountnew = count($rrdnewxml['rra']);
	/* process each RRA, which contain a database */
	foreach($rrdnewxml['rra'] as $rra) {
		if(!is_array($rrdoldxml['rra'][$i])) {
			$rrdoldxml['rra'][$i] = $rrdnewxml['rra'][$i];
		}

		$d = 0;
		/* process cdp_prep */
		$cdp_prep = $rra['cdp_prep'];
		foreach($cdp_prep['ds'] as $ds) {
			if(!is_array($rrdoldxml['rra'][$i]['cdp_prep']['ds'][$d])) {
				$rrdoldxml['rra'][$i]['cdp_prep']['ds'][$d] = $rrdnewxml['rra'][$i]['cdp_prep']['ds'][$d];
				$rrdoldxml['rra'][$i]['cdp_prep']['ds'][$d]['primary_value'] = " 0.0000000000e+00 ";
				$rrdoldxml['rra'][$i]['cdp_prep']['ds'][$d]['secondary_value'] = " 0.0000000000e+00 ";
				$rrdoldxml['rra'][$i]['cdp_prep']['ds'][$d]['value'] = " 0.0000000000e+00 ";
				$rrdoldxml['rra'][$i]['cdp_prep']['ds'][$d]['unknown_datapoints'] = "0";
			}
			$d++;
		}

		/* process database */
		$rows = $rra['database'];
		$k = 0;
		$rowcountold = count($rrdoldxml['rra'][$i]['database']['row']);
		$rowcountnew = count($rrdnewxml['rra'][$i]['database']['row']);
		$rowcountdiff = $rowcountnew - $rowcountold;
		/* save old rows for a bit before we put the required empty rows before it */
		$rowsdata = $rows;
		$rowsempty = array();
		$r = 0;
		while($r < $rowcountdiff) {
			$rowsempty[] = $rrdnewxml['rra'][$i]['database']['row'][$r];
			$r++;
		}
		$rows = $rowsempty + $rowsdata;
		/* now foreach the rows in the database */
		foreach($rows['row'] as $row) {
			if(!is_array($rrdoldxml['rra'][$i]['database']['row'][$k])) {
				$rrdoldxml['rra'][$i]['database']['row'][$k] = $rrdnewxml['rra'][$i]['database']['row'][$k];
			}
			$m = 0;
			$vcountold = count($rrdoldxml['rra'][$i]['database']['row'][$k]['v']);
			$vcountnew = count($rrdnewxml['rra'][$i]['database']['row'][$k]['v']);
			foreach($row['v'] as $value) {
				if(empty($rrdoldxml['rra'][$i]['database']['row'][$k]['v'][$m])) {
					if(isset($valid)) {
						$rrdoldxml['rra'][$i]['database']['row'][$k]['v'][$m] = "0.0000000000e+00 ";
					} else {
						$rrdoldxml['rra'][$i]['database']['row'][$k]['v'][$m] = $rrdnewxml['rra'][$i]['database']['row'][$k]['v'][$m];
					}
				} else {
					if($value <> " NaN ") {
						$valid = true;
					} else {
						$valid = false;
					}
				}
				$m++;
			}
			$k++;
		}
		$i++;
	}

	$numrranew = count($rrdoldxml['rra']);
	$numdsnew = count($rrdoldxml['ds']);
	log_error(sprintf(gettext('The new RRD now has %1$s DS values and %2$s RRA databases'), $numdsnew, $numrranew));
	return $rrdoldxml;
}

function enable_rrd_graphing() {
	global $config, $g, $altq_list_queues;

	if($g['booting']) 
		echo gettext("Generating RRD graphs...");

	$rrddbpath = "/var/db/rrd/";
	$rrdgraphpath = "/usr/local/www/rrd";

	$traffic = "-traffic.rrd";
	$packets = "-packets.rrd";
	$states = "-states.rrd";
	$wireless = "-wireless.rrd";
	$queues = "-queues.rrd";
	$queuesdrop = "-queuedrops.rrd";
	$spamd = "-spamd.rrd";
	$proc = "-processor.rrd";
	$mem = "-memory.rrd";
	$cellular = "-cellular.rrd";
	$vpnusers = "-vpnusers.rrd";
	$captiveportalconcurrent = "-concurrent.rrd";
	$captiveportalloggedin = "-loggedin.rrd";
	$captiveportaltotalusers = "-totalusers.rrd";

	$rrdtool = "/usr/bin/nice -n20 /usr/local/bin/rrdtool";
	$netstat = "/usr/bin/netstat";
	$awk = "/usr/bin/awk";
	$tar = "/usr/bin/tar";
	$pfctl = "/sbin/pfctl";
	$sysctl = "/sbin/sysctl";
	$php = "/usr/local/bin/php";
	$cpustats = "/usr/local/sbin/cpustats";
	$spamd_gather = "/usr/local/bin/spamd_gather_stats.php";
	$ifconfig = "/sbin/ifconfig";
	$captiveportal_gather = "/usr/local/bin/captiveportal_gather_stats.php";

	$rrdtrafficinterval = 60;
	$rrdwirelessinterval = 60;
	$rrdqueuesinterval = 60;
	$rrdqueuesdropinterval = 60;
	$rrdpacketsinterval = 60;
	$rrdstatesinterval = 60;
	$rrdspamdinterval = 60;
	$rrdlbpoolinterval = 60;
	$rrdprocinterval = 60;
	$rrdmeminterval = 60;
	$rrdcellularinterval = 60;
	$rrdvpninterval = 60;
	$rrdcaptiveportalinterval = 60;

	$trafficvalid = $rrdtrafficinterval * 2;
	$wirelessvalid = $rrdwirelessinterval * 2;
	$queuesvalid = $rrdqueuesinterval * 2;
	$queuesdropvalid = $rrdqueuesdropinterval * 2;
	$packetsvalid = $rrdpacketsinterval * 2;
	$statesvalid = $rrdstatesinterval*2;
	$spamdvalid = $rrdspamdinterval * 2;
	$lbpoolvalid = $rrdlbpoolinterval * 2;
	$procvalid = $rrdlbpoolinterval * 2;
	$memvalid = $rrdmeminterval * 2;
	$cellularvalid = $rrdcellularinterval * 2;
	$vpnvalid = $rrdvpninterval * 2;
	$captiveportalvalid = $rrdcaptiveportalinterval * 2;

	/* Asume GigE for now */
	$downstream = 125000000;
	$upstream = 125000000;

	/* read the shaper config */
	read_altq_config();

	if (isset ($config['rrd']['enable'])) {

		/* create directory if needed */
		if (!is_dir($rrddbpath)) {
			mkdir($rrddbpath, 0775);
		}
		chown($rrddbpath, "nobody");

		if ($g['booting']) {
			if ($g['platform'] != "pfSense") {
				restore_rrd();
			}
		}

		/* db update script */
		$rrdupdatesh = "#!/bin/sh\n";
		$rrdupdatesh .= "\n";
		$rrdupdatesh .= "export TERM=dumb\n";
		$rrdupdatesh .= "counter=1\n";
		$rrdupdatesh .= "while [ \"\$counter\" -ne 0 ]\n";
		$rrdupdatesh .= "do\n";
		$rrdupdatesh .= "";

		$i = 0;
		$ifdescrs = get_configured_interface_with_descr();
		/* IPsec counters */
		$ifdescrs['ipsec'] = "IPsec";
		/* OpenVPN server counters */
		if(is_array($config['openvpn']['openvpn-server'])) {
			foreach($config['openvpn']['openvpn-server'] as $server) {
				$serverid = "ovpns" . $server['vpnid'];
				$ifdescrs[$serverid] = "{$server['description']}";
			}
		}

		/* process all real and pseudo interfaces */
		foreach ($ifdescrs as $ifname => $ifdescr) {
			$temp = get_real_interface($ifname);
			if($temp <> "") {
				$realif = $temp;
			}

			/* TRAFFIC, set up the rrd file */
			if (!file_exists("$rrddbpath$ifname$traffic")) {
				$rrdcreate = "$rrdtool create $rrddbpath$ifname$traffic --step $rrdtrafficinterval ";
				$rrdcreate .= "DS:inpass:COUNTER:$trafficvalid:0:$downstream ";
				$rrdcreate .= "DS:outpass:COUNTER:$trafficvalid:0:$upstream ";
				$rrdcreate .= "DS:inblock:COUNTER:$trafficvalid:0:$downstream ";
				$rrdcreate .= "DS:outblock:COUNTER:$trafficvalid:0:$upstream ";
				$rrdcreate .= "DS:inpass6:COUNTER:$trafficvalid:0:$downstream ";
				$rrdcreate .= "DS:outpass6:COUNTER:$trafficvalid:0:$upstream ";
				$rrdcreate .= "DS:inblock6:COUNTER:$trafficvalid:0:$downstream ";
				$rrdcreate .= "DS:outblock6:COUNTER:$trafficvalid:0:$upstream ";
				$rrdcreate .= "RRA:AVERAGE:0.5:1:1000 ";
				$rrdcreate .= "RRA:AVERAGE:0.5:5:1000 ";
				$rrdcreate .= "RRA:AVERAGE:0.5:60:1000 ";
				$rrdcreate .= "RRA:AVERAGE:0.5:720:3000 ";

				create_new_rrd($rrdcreate);
			}

			/* enter UNKNOWN values in the RRD so it knows we rebooted. */
			if($g['booting']) {
				mwexec("$rrdtool update $rrddbpath$ifname$traffic N:U:U:U:U:U:U:U:U");
			}

			$rrdupdatesh .= "\n";
			$rrdupdatesh .= "# polling traffic for interface $ifname $realif IPv4/IPv6 counters \n";
			$rrdupdatesh .= "$rrdtool update $rrddbpath$ifname$traffic N:";
			$rrdupdatesh .= "`$pfctl -vvsI -i {$realif} | awk '\\\n";
			$rrdupdatesh .= "/In4\/Pass/ { b4pi = \$6 };/Out4\/Pass/ { b4po = \$6 };/In4\/Block/ { b4bi = \$6 };/Out4\/Block/ { b4bo = \$6 };\\\n";
			$rrdupdatesh .= "/In6\/Pass/ { b6pi = \$6 };/Out6\/Pass/ { b6po = \$6 };/In6\/Block/ { b6bi = \$6 };/Out6\/Block/ { b6bo = \$6 };\\\n";
			$rrdupdatesh .= "END {print b4pi \":\" b4po \":\" b4bi \":\" b4bo \":\" b6pi \":\" b6po \":\" b6bi \":\" b6bo};'`\n";

			/* PACKETS, set up the rrd file */
			if (!file_exists("$rrddbpath$ifname$packets")) {
				$rrdcreate = "$rrdtool create $rrddbpath$ifname$packets --step $rrdpacketsinterval ";
				$rrdcreate .= "DS:inpass:COUNTER:$packetsvalid:0:$downstream ";
				$rrdcreate .= "DS:outpass:COUNTER:$packetsvalid:0:$upstream ";
				$rrdcreate .= "DS:inblock:COUNTER:$packetsvalid:0:$downstream ";
				$rrdcreate .= "DS:outblock:COUNTER:$packetsvalid:0:$upstream ";
				$rrdcreate .= "DS:inpass6:COUNTER:$packetsvalid:0:$downstream ";
				$rrdcreate .= "DS:outpass6:COUNTER:$packetsvalid:0:$upstream ";
				$rrdcreate .= "DS:inblock6:COUNTER:$packetsvalid:0:$downstream ";
				$rrdcreate .= "DS:outblock6:COUNTER:$packetsvalid:0:$upstream ";
				$rrdcreate .= "RRA:AVERAGE:0.5:1:1000 ";
				$rrdcreate .= "RRA:AVERAGE:0.5:5:1000 ";
				$rrdcreate .= "RRA:AVERAGE:0.5:60:1000 ";
				$rrdcreate .= "RRA:AVERAGE:0.5:720:3000 ";

				create_new_rrd($rrdcreate);
			}

			/* enter UNKNOWN values in the RRD so it knows we rebooted. */
			if($g['booting']) {
				mwexec("$rrdtool update $rrddbpath$ifname$packets N:U:U:U:U:U:U:U:U");
			}

			$rrdupdatesh .= "\n";
			$rrdupdatesh .= "# polling packets for interface $ifname $realif \n";
			$rrdupdatesh .= "$rrdtool update $rrddbpath$ifname$packets N:";
			$rrdupdatesh .= "`$pfctl -vvsI -i {$realif} | awk '\\\n";
			$rrdupdatesh .= "/In4\/Pass/ { b4pi = \$4 };/Out4\/Pass/ { b4po = \$4 };/In4\/Block/ { b4bi = \$4 };/Out4\/Block/ { b4bo = \$4 };\\\n";
			$rrdupdatesh .= "/In6\/Pass/ { b6pi = \$4 };/Out6\/Pass/ { b6po = \$4 };/In6\/Block/ { b6bi = \$4 };/Out6\/Block/ { b6bo = \$4 };\\\n";
			$rrdupdatesh .= "END {print b4pi \":\" b4po \":\" b4bi \":\" b4bo \":\" b6pi \":\" b6po \":\" b6bi \":\" b6bo};'`\n";

			/* WIRELESS, set up the rrd file */
			if($config['interfaces'][$ifname]['wireless']['mode'] == "bss") {
				if (!file_exists("$rrddbpath$ifname$wireless")) {
					$rrdcreate = "$rrdtool create $rrddbpath$ifname$wireless --step $rrdwirelessinterval ";
					$rrdcreate .= "DS:snr:GAUGE:$wirelessvalid:0:1000 ";
					$rrdcreate .= "DS:rate:GAUGE:$wirelessvalid:0:1000 ";
					$rrdcreate .= "DS:channel:GAUGE:$wirelessvalid:0:1000 ";
					$rrdcreate .= "RRA:AVERAGE:0.5:1:1000 ";
					$rrdcreate .= "RRA:AVERAGE:0.5:5:1000 ";
					$rrdcreate .= "RRA:AVERAGE:0.5:60:1000 ";
					$rrdcreate .= "RRA:AVERAGE:0.5:720:3000 ";
	
					create_new_rrd($rrdcreate);
				}

				/* enter UNKNOWN values in the RRD so it knows we rebooted. */
				if($g['booting']) {
					mwexec("$rrdtool update $rrddbpath$ifname$wireless N:U:U:U");
				}

				$rrdupdatesh .= "\n";
				$rrdupdatesh .= "# polling wireless for interface $ifname $realif \n";
				$rrdupdatesh .= "WIFI=`$ifconfig {$realif} list sta| $awk 'gsub(\"M\", \"\") {getline 2;print substr(\$5, 0, length(\$5)-2) \":\" $4 \":\" $3}'`\n";
				$rrdupdatesh .= "$rrdtool update $rrddbpath$ifname$wireless N:${WIFI}\n";
			}

			/* OpenVPN, set up the rrd file */
			if(stristr($ifname, "ovpns")) {
				if (!file_exists("$rrddbpath$ifname$vpnusers")) {
					$rrdcreate = "$rrdtool create $rrddbpath$ifname$vpnusers --step $rrdvpninterval ";
					$rrdcreate .= "DS:users:GAUGE:$vpnvalid:0:10000 ";
					$rrdcreate .= "RRA:AVERAGE:0.5:1:1000 ";
					$rrdcreate .= "RRA:AVERAGE:0.5:5:1000 ";
					$rrdcreate .= "RRA:AVERAGE:0.5:60:1000 ";
					$rrdcreate .= "RRA:AVERAGE:0.5:720:3000 ";
	
					create_new_rrd($rrdcreate);
				}

				/* enter UNKNOWN values in the RRD so it knows we rebooted. */
				if($g['booting']) {
					mwexec("$rrdtool update $rrddbpath$ifname$vpnusers N:U");
				}

				if(is_array($config['openvpn']['openvpn-server'])) {
					foreach($config['openvpn']['openvpn-server'] as $server) {
						if("ovpns{$server['vpnid']}" == $ifname) {
							$port = $server['local_port'];
							$vpnid = $server['vpnid'];
						}
					}
				}
				$rrdupdatesh .= "\n";
				$rrdupdatesh .= "# polling vpn users for interface $ifname $realif port $port\n";
				$rrdupdatesh .= "list_current_users() {\n";
				$rrdupdatesh .= " sleep 0.2\n";
				$rrdupdatesh .= " echo \"status 2\"\n";
				$rrdupdatesh .= " sleep 0.2\n";
				$rrdupdatesh .= " echo \"quit\"\n";
				$rrdupdatesh .= "}\n";
				$rrdupdatesh .= "OVPN=`list_current_users | nc -U {$g['varetc_path']}/openvpn/server{$vpnid}.sock | awk -F\",\" '/^CLIENT_LIST/ {print \$2}' | wc -l | awk '{print $1}'`\n";
				$rrdupdatesh .= "$rrdtool update $rrddbpath$ifname$vpnusers N:\${OVPN}\n";
			}

			/* QUEUES, set up the queues databases */
			if ($altq_list_queues[$ifname]) {
				$altq =& $altq_list_queues[$ifname];
				/* NOTE: Is it worth as its own function?! */
				switch ($altq->GetBwscale()) {
					case "Gb":
						$factor = 1024 * 1024 * 1024;
							break;
					case "Mb":
							$factor = 1024 * 1024;
							break;
					case "Kb":
							$factor = 1024;
							break;
					case "b":
					default:
							$factor = 1;
							break;
				}
				$qbandwidth = $altq->GetBandwidth() * $factor;
				if ($qbandwidth <=0) {
					$qbandwidth = 100 * 1000 * 1000; /* 100Mbit */
				}
				$qlist =& $altq->get_queue_list($notused);
				if (!file_exists("$rrddbpath$ifname$queues")) {
					$rrdcreate = "$rrdtool create $rrddbpath$ifname$queues --step $rrdqueuesinterval ";
					/* loop list of shaper queues */
					$q = 0;
					foreach ($qlist as $qname => $q) {
						$rrdcreate .= "DS:$qname:COUNTER:$queuesvalid:0:$qbandwidth ";
					}

					$rrdcreate .= "RRA:AVERAGE:0.5:1:1000 ";
					$rrdcreate .= "RRA:AVERAGE:0.5:5:1000 ";
					$rrdcreate .= "RRA:AVERAGE:0.5:60:1000 ";
					$rrdcreate .= "RRA:AVERAGE:0.5:720:3000 ";

					create_new_rrd($rrdcreate);
				}

				if (!file_exists("$rrddbpath$ifname$queuesdrop")) {
					$rrdcreate = "$rrdtool create $rrddbpath$ifname$queuesdrop --step $rrdqueuesdropinterval ";
					/* loop list of shaper queues */
					$q = 0;
					foreach ($qlist as $qname => $q) {
						$rrdcreate .= "DS:$qname:COUNTER:$queuesdropvalid:0:$qbandwidth ";
					}

					$rrdcreate .= "RRA:AVERAGE:0.5:1:1000 ";
					$rrdcreate .= "RRA:AVERAGE:0.5:5:1000 ";
					$rrdcreate .= "RRA:AVERAGE:0.5:60:1000 ";
					$rrdcreate .= "RRA:AVERAGE:0.5:720:3000 ";

					create_new_rrd($rrdcreate);
				}

				if($g['booting']) {
					$rrdqcommand = "-t ";
					$rrducommand = "N";
					$qi = 0;
					foreach ($qlist as $qname => $q) {
						if($qi == 0) {
							$rrdqcommand .= "{$qname}";
						} else {
							$rrdqcommand .= ":{$qname}";
						}
						$qi++;
						$rrducommand .= ":U";
					}
					mwexec("$rrdtool update $rrddbpath$ifname$queues $rrdqcommand $rrducommand");
					mwexec("$rrdtool update $rrddbpath$ifname$queuesdrop $rrdqcommand $rrducommand");
				}

				/* awk function to gather shaper data */
				/* yes, it's special */
				$rrdupdatesh .= "` pfctl -vsq -i {$realif} | awk 'BEGIN {printf \"$rrdtool update $rrddbpath$ifname$queues \" } ";
				$rrdupdatesh .= "{ ";
				$rrdupdatesh .= "if ((\$1 == \"queue\") && ( \$2 ~ /^q/ )) { ";
				$rrdupdatesh .= " dsname = dsname \":\" \$2 ; ";
				$rrdupdatesh .= " q=1; ";
				$rrdupdatesh .= "} ";
				$rrdupdatesh .= " else if ((\$4 == \"bytes:\") && ( q == 1 ) ) { ";
				$rrdupdatesh .= " dsdata = dsdata \":\" \$5 ; ";
				$rrdupdatesh .= " q=0; ";
				$rrdupdatesh .= "} ";
				$rrdupdatesh .= "} END { ";
				$rrdupdatesh .= " dsname = substr(dsname,2); ";
				$rrdupdatesh .= " dsdata = substr(dsdata,2); ";
				$rrdupdatesh .= " printf \"-t \" dsname \" N:\" dsdata }' ";
				$rrdupdatesh .= " dsname=\"\" dsdata=\"\"`\n\n";

				$rrdupdatesh .= "` pfctl -vsq -i {$realif} | awk 'BEGIN {printf \"$rrdtool update $rrddbpath$ifname$queuesdrop \" } ";
				$rrdupdatesh .= "{ ";
				$rrdupdatesh .= "if ((\$1 == \"queue\") && ( \$2 ~ /^q/ )) { ";
				$rrdupdatesh .= " dsname = dsname \":\" \$2 ; ";
				$rrdupdatesh .= " q=1; ";
				$rrdupdatesh .= "} ";
				$rrdupdatesh .= " else if ((\$4 == \"bytes:\") && ( q == 1 ) ) { ";
				$rrdupdatesh .= " dsdata = dsdata \":\" \$8 ; ";
				$rrdupdatesh .= " q=0; ";
				$rrdupdatesh .= "} ";
				$rrdupdatesh .= "} END { ";
				$rrdupdatesh .= " dsname = substr(dsname,2); ";
				$rrdupdatesh .= " dsdata = substr(dsdata,2); ";
				$rrdupdatesh .= " printf \"-t \" dsname \" N:\" dsdata }' ";
				$rrdupdatesh .= " dsname=\"\" dsdata=\"\"`\n\n";
			}

			/* 3G interfaces */
			if(preg_match("/ppp[0-9]+/i", $realif))	{
				if (!file_exists("$rrddbpath$ifname$cellular")) {
					$rrdcreate = "$rrdtool create $rrddbpath$ifname$cellular --step $rrdcellularinterval ";
					$rrdcreate .= "DS:rssi:GAUGE:$cellularvalid:0:100 ";
					$rrdcreate .= "DS:upstream:GAUGE:$cellularvalid:0:100000000 ";
					$rrdcreate .= "DS:downstream:GAUGE:$cellularvalid:0:100000000 ";
					$rrdcreate .= "RRA:AVERAGE:0.5:1:1000 ";
					$rrdcreate .= "RRA:AVERAGE:0.5:5:1000 ";
					$rrdcreate .= "RRA:AVERAGE:0.5:60:1000 ";
					$rrdcreate .= "RRA:AVERAGE:0.5:720:3000 ";
					create_new_rrd($rrdcreate);
				}

				/* enter UNKNOWN values in the RRD so it knows we rebooted. */
				if($g['booting']) {
					mwexec("$rrdtool update $rrddbpath$ifname$cellular N:U:U:U");
				}

				$rrdupdatesh .= "\n";
				$rrdupdatesh .= "# polling 3G\n";
				$rrdupdatesh .= "GSTATS=`awk -F, 'getline 2 {print \$2 \":\" \$8 \":\" \$9}' < /tmp/3gstats.$ifname`\n";
				$rrdupdatesh .= "$rrdtool update $rrddbpath$ifname$cellular N:\"\$GSTATS\"";
			}

		}
		$i++;

		/* System only statistics */
		$ifname = "system";

		/* STATES, create pf states database */
		if(! file_exists("$rrddbpath$ifname$states")) {
			$rrdcreate = "$rrdtool create $rrddbpath$ifname$states --step $rrdstatesinterval ";
			$rrdcreate .= "DS:pfrate:GAUGE:$statesvalid:0:10000000 ";
			$rrdcreate .= "DS:pfstates:GAUGE:$statesvalid:0:10000000 ";
			$rrdcreate .= "DS:pfnat:GAUGE:$statesvalid:0:10000000 ";
			$rrdcreate .= "DS:srcip:GAUGE:$statesvalid:0:10000000 ";
			$rrdcreate .= "DS:dstip:GAUGE:$statesvalid:0:10000000 ";
			$rrdcreate .= "RRA:AVERAGE:0.5:1:1000 ";
			$rrdcreate .= "RRA:AVERAGE:0.5:5:1000 ";
			$rrdcreate .= "RRA:AVERAGE:0.5:60:1000 ";
			$rrdcreate .= "RRA:AVERAGE:0.5:720:3000 ";

			create_new_rrd($rrdcreate);
		}

		/* enter UNKNOWN values in the RRD so it knows we rebooted. */
		if($g['booting']) {
			mwexec("$rrdtool update $rrddbpath$ifname$states N:U:U:U:U:U");
		}

		/* the pf states gathering function. */
		$rrdupdatesh .= "\n";
		$rrdupdatesh .= "pfctl_si_out=\"` $pfctl -si > /tmp/pfctl_si_out `\"\n";
		$rrdupdatesh .= "pfctl_ss_out=\"` $pfctl -ss > /tmp/pfctl_ss_out`\"\n";
		$rrdupdatesh .= "pfrate=\"` cat /tmp/pfctl_si_out | egrep \"inserts|removals\" | awk '{ pfrate = \$3 + pfrate } {print pfrate}'|tail -1 `\"\n";
		$rrdupdatesh .= "pfstates=\"` cat /tmp/pfctl_ss_out | egrep -v \"<\\-.*?<\\-|\\->.*?\\->\" | wc -l|sed 's/ //g'`\"\n";
		$rrdupdatesh .= "pfnat=\"` cat /tmp/pfctl_ss_out | egrep '<\\-.*?<\\-|\\->.*?\\->' | wc -l|sed 's/ //g' `\"\n";
		$rrdupdatesh .= "srcip=\"` cat /tmp/pfctl_ss_out | egrep -v '<\\-.*?<\\-|\\->.*?\\->' | grep '\\->' | awk '{print \$3}' | awk -F: '{print \$1}' | sort -u|wc -l|sed 's/ //g' `\"\n";
		$rrdupdatesh .= "dstip=\"` cat /tmp/pfctl_ss_out | egrep -v '<\\-.*?<\\-|\\->.*?\\->' | grep '<\\-' | awk '{print \$3}' | awk -F: '{print \$1}' | sort -u|wc -l|sed 's/ //g' `\"\n";
		$rrdupdatesh .= "$rrdtool update $rrddbpath$ifname$states N:\$pfrate:\$pfstates:\$pfnat:\$srcip:\$dstip\n\n";

		/* End pf states statistics */

		/* CPU, create CPU statistics database */
		if(! file_exists("$rrddbpath$ifname$proc")) {
			$rrdcreate = "$rrdtool create $rrddbpath$ifname$proc --step $rrdprocinterval ";
			$rrdcreate .= "DS:user:GAUGE:$procvalid:0:10000000 ";
			$rrdcreate .= "DS:nice:GAUGE:$procvalid:0:10000000 ";
			$rrdcreate .= "DS:system:GAUGE:$procvalid:0:10000000 ";
			$rrdcreate .= "DS:interrupt:GAUGE:$procvalid:0:10000000 ";
			$rrdcreate .= "DS:processes:GAUGE:$procvalid:0:10000000 ";
			$rrdcreate .= "RRA:AVERAGE:0.5:1:1000 ";
			$rrdcreate .= "RRA:AVERAGE:0.5:5:1000 ";
			$rrdcreate .= "RRA:AVERAGE:0.5:60:1000 ";
			$rrdcreate .= "RRA:AVERAGE:0.5:720:3000 ";

			create_new_rrd($rrdcreate);
		}

		/* enter UNKNOWN values in the RRD so it knows we rebooted. */
		if($g['booting']) {
			mwexec("$rrdtool update $rrddbpath$ifname$proc N:U:U:U:U:U");
		}

		/* the CPU stats gathering function. */
		$rrdupdatesh .= "CPU=`$cpustats | cut -f1-4 -d':'`\n";
		/* Using ps uxaH will count all processes including system threads. Top was undercounting. */
		$rrdupdatesh .= "PROCS=`ps uxaH | wc -l | awk '{print \$1;}'`\n";
		$rrdupdatesh .= "$rrdtool update $rrddbpath$ifname$proc N:\${CPU}:\${PROCS}\n";

		/* End CPU statistics */

		/* Memory, create Memory statistics database */
		if(! file_exists("$rrddbpath$ifname$mem")) {
			$rrdcreate = "$rrdtool create $rrddbpath$ifname$mem --step $rrdmeminterval ";
			$rrdcreate .= "DS:active:GAUGE:$memvalid:0:10000000 ";
			$rrdcreate .= "DS:inactive:GAUGE:$memvalid:0:10000000 ";
			$rrdcreate .= "DS:free:GAUGE:$memvalid:0:10000000 ";
			$rrdcreate .= "DS:cache:GAUGE:$memvalid:0:10000000 ";
			$rrdcreate .= "DS:wire:GAUGE:$memvalid:0:10000000 ";
			$rrdcreate .= "RRA:MIN:0.5:1:1000 ";
			$rrdcreate .= "RRA:MIN:0.5:5:1000 ";
			$rrdcreate .= "RRA:MIN:0.5:60:1000 ";
			$rrdcreate .= "RRA:MIN:0.5:720:3000 ";
			$rrdcreate .= "RRA:AVERAGE:0.5:1:1000 ";
			$rrdcreate .= "RRA:AVERAGE:0.5:5:1000 ";
			$rrdcreate .= "RRA:AVERAGE:0.5:60:1000 ";
			$rrdcreate .= "RRA:AVERAGE:0.5:720:3000 ";
			$rrdcreate .= "RRA:MAX:0.5:1:1000 ";
			$rrdcreate .= "RRA:MAX:0.5:5:1000 ";
			$rrdcreate .= "RRA:MAX:0.5:60:1000 ";
			$rrdcreate .= "RRA:MAX:0.5:720:3000";

			create_new_rrd($rrdcreate);
		}

		/* enter UNKNOWN values in the RRD so it knows we rebooted. */
		if($g['booting']) {
			mwexec("$rrdtool update $rrddbpath$ifname$mem N:U:U:U:U:U");
		}

		/* the Memory stats gathering function. */
		$rrdupdatesh .= "MEM=`$sysctl -n vm.stats.vm.v_page_count vm.stats.vm.v_active_count vm.stats.vm.v_inactive_count vm.stats.vm.v_free_count vm.stats.vm.v_cache_count vm.stats.vm.v_wire_count | ";
		$rrdupdatesh .= " $awk '{getline active;getline inactive;getline free;getline cache;getline wire;printf ";
		$rrdupdatesh .= "((active/$0) * 100)\":\"((inactive/$0) * 100)\":\"((free/$0) * 100)\":\"((cache/$0) * 100)\":\"(wire/$0 * 100)}'`\n";
		$rrdupdatesh .= "$rrdtool update $rrddbpath$ifname$mem N:\${MEM}\n";
		
		/* End Memory statistics */

		/* SPAMD, set up the spamd rrd file */
		if (isset($config['installedpackages']['spamdsettings']) &&
			 $config['installedpackages']['spamdsettings']['config'][0]['enablerrd']) {
			/* set up the spamd rrd file */
			if (!file_exists("$rrddbpath$ifname$spamd")) {
				$rrdcreate = "$rrdtool create $rrddbpath$ifname$spamd --step $rrdspamdinterval ";
				$rrdcreate .= "DS:conn:GAUGE:$spamdvalid:0:10000 ";
				$rrdcreate .= "DS:time:GAUGE:$spamdvalid:0:86400 ";
				$rrdcreate .= "RRA:MIN:0.5:1:1000 ";
				$rrdcreate .= "RRA:MIN:0.5:5:1000 ";
				$rrdcreate .= "RRA:MIN:0.5:60:1000 ";
				$rrdcreate .= "RRA:MIN:0.5:720:3000 ";
				$rrdcreate .= "RRA:AVERAGE:0.5:1:1000 ";
				$rrdcreate .= "RRA:AVERAGE:0.5:5:1000 ";
				$rrdcreate .= "RRA:AVERAGE:0.5:60:1000 ";
				$rrdcreate .= "RRA:AVERAGE:0.5:720:3000 ";
				$rrdcreate .= "RRA:MAX:0.5:1:1000 ";
				$rrdcreate .= "RRA:MAX:0.5:5:1000 ";
				$rrdcreate .= "RRA:MAX:0.5:60:1000 ";
				$rrdcreate .= "RRA:MAX:0.5:720:3000 ";

				create_new_rrd($rrdcreate);
			}

			$rrdupdatesh .= "\n";
			$rrdupdatesh .= "# polling spamd for connections and tarpitness \n";
			$rrdupdatesh .= "$rrdtool update $rrddbpath$ifname$spamd \\\n";
			$rrdupdatesh .= "`$php -q $spamd_gather`\n";

		}
		/* End System statistics */

		/* Captive Portal statistics, set up the rrd file */
		if(isset($config['captiveportal']['enable'])) {
			$ifname= "captiveportal";
			if (!file_exists("$rrddbpath$ifname$captiveportalconcurrent")) {
				$rrdcreate = "$rrdtool create $rrddbpath$ifname$captiveportalconcurrent --step $rrdcaptiveportalinterval ";
				$rrdcreate .= "DS:concurrentusers:GAUGE:$captiveportalvalid:0:10000 ";
				$rrdcreate .= "RRA:AVERAGE:0.5:1:1000 ";
				$rrdcreate .= "RRA:AVERAGE:0.5:5:1000 ";
				$rrdcreate .= "RRA:AVERAGE:0.5:60:1000 ";
				$rrdcreate .= "RRA:AVERAGE:0.5:720:3000 ";
				$rrdcreate .= "RRA:MIN:0.5:1:1000 ";
				$rrdcreate .= "RRA:MIN:0.5:5:1000 ";
				$rrdcreate .= "RRA:MIN:0.5:60:1000 ";
				$rrdcreate .= "RRA:MIN:0.5:720:3000 ";
				$rrdcreate .= "RRA:MAX:0.5:1:1000 ";
				$rrdcreate .= "RRA:MAX:0.5:5:1000 ";
				$rrdcreate .= "RRA:MAX:0.5:60:1000 ";
				$rrdcreate .= "RRA:MAX:0.5:720:3000 ";
				$rrdcreate .= "RRA:LAST:0.5:1:1000 ";
				$rrdcreate .= "RRA:LAST:0.5:5:1000 ";
				$rrdcreate .= "RRA:LAST:0.5:60:1000 ";
				$rrdcreate .= "RRA:LAST:0.5:720:3000 ";

				create_new_rrd($rrdcreate);
			}

			/* enter UNKNOWN values in the RRD so it knows we rebooted. */
			if($g['booting']) {
				mwexec("$rrdtool update $rrddbpath$ifname$captiveportalconcurrent N:U");
			}
			
			/* the Captive Portal stats gathering function. */
			$rrdupdatesh .= "\n";
			$rrdupdatesh .= "# polling Captive Portal for number of concurrent users\n";
			$rrdupdatesh .= "CP=`$php -q $captiveportal_gather concurrent`\n";
			$rrdupdatesh .= "$rrdtool update $rrddbpath$ifname$captiveportalconcurrent \${CP}\n";
			
			$ifname= "captiveportal";
			if (!file_exists("$rrddbpath$ifname$captiveportalloggedin")) {
				$rrdcreate = "$rrdtool create $rrddbpath$ifname$captiveportalloggedin --step $rrdcaptiveportalinterval ";
				$rrdcreate .= "DS:loggedinusers:GAUGE:$captiveportalvalid:0:10000 ";
				$rrdcreate .= "RRA:AVERAGE:0.5:1:1000 ";
				$rrdcreate .= "RRA:AVERAGE:0.5:5:1000 ";
				$rrdcreate .= "RRA:AVERAGE:0.5:60:1000 ";
				$rrdcreate .= "RRA:AVERAGE:0.5:720:3000 ";
				$rrdcreate .= "RRA:MIN:0.5:1:1000 ";
				$rrdcreate .= "RRA:MIN:0.5:5:1000 ";
				$rrdcreate .= "RRA:MIN:0.5:60:1000 ";
				$rrdcreate .= "RRA:MIN:0.5:720:3000 ";
				$rrdcreate .= "RRA:MAX:0.5:1:1000 ";
				$rrdcreate .= "RRA:MAX:0.5:5:1000 ";
				$rrdcreate .= "RRA:MAX:0.5:60:1000 ";
				$rrdcreate .= "RRA:MAX:0.5:720:3000 ";
				$rrdcreate .= "RRA:LAST:0.5:1:1000 ";
				$rrdcreate .= "RRA:LAST:0.5:5:1000 ";
				$rrdcreate .= "RRA:LAST:0.5:60:1000 ";
				$rrdcreate .= "RRA:LAST:0.5:720:3000 ";

				create_new_rrd($rrdcreate);
			}

			/* enter UNKNOWN values in the RRD so it knows we rebooted. */
			if($g['booting']) {
				mwexec("$rrdtool update $rrddbpath$ifname$captiveportalloggedin N:U");
			}

			/* the Captive Portal stats gathering function. */
			$rrdupdatesh .= "\n";
			$rrdupdatesh .= "# polling Captive Portal for number of logged in users\n";
			$rrdupdatesh .= "CP=`$php -q $captiveportal_gather loggedin`\n";
			$rrdupdatesh .= "$rrdtool update $rrddbpath$ifname$captiveportalloggedin \${CP}\n";

		}

		$rrdupdatesh .= "sleep 60\n";
		$rrdupdatesh .= "done\n";
		log_error(gettext("Creating rrd update script"));
		/* write the rrd update script */
		$updaterrdscript = "{$g['vardb_path']}/rrd/updaterrd.sh";
		$fd = fopen("$updaterrdscript", "w");
		fwrite($fd, "$rrdupdatesh");
		fclose($fd);

		/* kill off traffic collectors */
		kill_traffic_collector();

		/* start traffic collector */
		mwexec_bg("/usr/bin/nice -n20 /bin/sh $updaterrdscript");

	} else {
		/* kill off traffic collectors */
		kill_traffic_collector();
	}

	$databases = glob("{$rrddbpath}/*.rrd");
	foreach($databases as $database) {
		chown($database, "nobody");
	}

	if($g['booting']) 
		echo gettext("done.") . "\n";
		
}

function kill_traffic_collector() {
	mwexec("killall rrdtool", true);
	mwexec("/bin/pkill -a -f updaterrd.sh", true);
}

?>

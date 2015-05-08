var ZeroTierNode = React.createClass({
	getInitialState: function() {
		return {
			address: '----------',
			online: false,
			version: '_._._',
			_networks: [],
			_peers: []
		};
	},

	ago: function(ms) {
		if (ms > 0) {
			var tmp = Math.round((Date.now() - ms) / 1000);
			return ((tmp > 0) ? tmp : 0);
		} else return 0;
	},

	updatePeers: function() {
		Ajax.call({
			url: 'peer?auth='+this.props.authToken,
			cache: false,
			type: 'GET',
			success: function(data) {
				if (data) {
					var pl = JSON.parse(data);
					if (Array.isArray(pl)) {
						this.setState({_peers: pl});
					}
				}
			}.bind(this),
			error: function() {
			}.bind(this)
		});
	},
	updateNetworks: function() {
		Ajax.call({
			url: 'network?auth='+this.props.authToken,
			cache: false,
			type: 'GET',
			success: function(data) {
				if (data) {
					var nwl = JSON.parse(data);
					if (Array.isArray(nwl)) {
						this.setState({_networks: nwl});
					}
				}
			}.bind(this),
			error: function() {
			}.bind(this)
		});
	},
	updateAll: function() {
		Ajax.call({
			url: 'status?auth='+this.props.authToken,
			cache: false,
			type: 'GET',
			success: function(data) {
				if (data) {
					var status = JSON.parse(data);
					this.setState(status);
					document.title = 'ZeroTier One [' + status.address + ']';
				}
				this.updateNetworks();
				this.updatePeers();
			}.bind(this),
			error: function() {
				this.setState({online: false});
			}.bind(this)
		});
	},
	joinNetwork: function(event) {
		event.preventDefault();
		if ((this.networkToJoin)&&(this.networkToJoin.length === 16)) {
			Ajax.call({
				url: 'network/'+this.networkToJoin+'?auth='+this.props.authToken,
				cache: false,
				type: 'POST',
				success: function(data) {
				}.bind(this),
				error: function() {
				}.bind(this)
			});
		}
	},
	handleNetworkIdEntry: function(event) {
		var nid = event.target.value;
		if (nid) {
			nid = nid.toLowerCase();
			var nnid = '';
			for(var i=0;((i<nid.length)&&(i<16));++i) {
				if ("0123456789abcdef".indexOf(nid.charAt(i)) >= 0)
					nnid += nid.charAt(i);
			}
			this.networkToJoin = nnid;
			event.target.value = nnid;
		} else {
			this.networkToJoin = '';
			event.target.value = '';
		}
	},

	componentDidMount: function() {
		this.tabIndex = 0;
		this.updateAll();
		this.updateIntervalId = setInterval(this.updateAll,2500);
	},
	componentWillUnmount: function() {
		clearInterval(this.updateIntervalId);
	},
	render: function() {
		return (
			<div className="zeroTierNode">
				<div className="top">&nbsp;&nbsp;
					<button disabled={this.tabIndex === 0} onClick={function() {this.tabIndex = 0; this.forceUpdate();}.bind(this)}>Networks</button>
					<button disabled={this.tabIndex === 1} onClick={function() {this.tabIndex = 1; this.forceUpdate();}.bind(this)}>Peers</button>
				</div>
				<div className="middle">
					<div className="middleScroll">
						{
							(this.tabIndex === 1) ? (
								<div className="peers">
									<div className="peerHeader">
										<div className="f">Address</div>
										<div className="f">Version</div>
										<div className="f">Latency</div>
										<div className="f">Direct&nbsp;Paths</div>
										<div className="f">Role</div>
									</div>
									{
										this.state._peers.map(function(peer) {
											return (
												<div className="peer">
													<div className="f zeroTierAddress">{peer['address']}</div>
													<div className="f">{(peer['version'] === '-1.-1.-1') ? '-' : peer['version']}</div>
													<div className="f">{peer['latency']}</div>
													<div className="f">
														{
															(peer['paths'].length === 0) ? (
																<div className="peerPath"><i>(none)</i></div>
															) : (
																<div>
																{
																	peer['paths'].map(function(path) {
																		if ((path.active)||(path.fixed)) {
																			return (
																				<div className="peerPath">{path.address}&nbsp;{this.ago(path.lastSend)}&nbsp;{this.ago(path.lastReceive)}{path.preferred ? ' *' : ''}</div>
																			);
																		} else {
																			return (
																				<div className="peerPathInactive">{path.address}&nbsp;{this.ago(path.lastSend)}&nbsp;{this.ago(path.lastReceive)}</div>
																			);
																		}
																	}.bind(this))
																}
																</div>
															)
														}
													</div>
													<div className="f">{peer['role']}</div>
												</div>
											);
										}.bind(this))
									}
								</div>
							) : (
								<div className="networks">
									{
										this.state._networks.map(function(network) {
											network['authToken'] = this.props.authToken;
											return React.createElement('div',{className: 'network'},React.createElement(ZeroTierNetwork,network));
										}.bind(this))
									}
								</div>
							)
						}
					</div>
				</div>
				<div className="bottom">
					<div className="left">
						<span className="statusLine"><span className="zeroTierAddress">{this.state.address}</span>&nbsp;&nbsp;{this.state.online ? 'ONLINE' : 'OFFLINE'}&nbsp;&nbsp;{this.state.version}</span>
					</div>
					<div className="right">
						<form onSubmit={this.joinNetwork}><input type="text" placeholder="################" onChange={this.handleNetworkIdEntry} size="16"/><button type="submit">Join</button></form>
					</div>
				</div>
			</div>
		);
	}
});
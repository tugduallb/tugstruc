﻿//Kitsunen's quick and dirty socket.io API connection for own uses.

// Show the 'loading' screen first when we try to connect to Pimatic
$("#body-message").html("Connecting to Pimatic API");
$("#loading").show();

//DEFINE THE CONNECTION
var socket = io.connect('http://192.168.2.145:8090/?username=admin&password=alitug', {
  reconnection: true,
  reconnectionDelay: 1000,
  reconnectionDelayMax: 3000,
  timeout: 20000,
  forceNew: true
});
//MESSAGE ON CONSOLE WHEN CONNECTED
socket.on('connect', function() {
  console.log('Connection to Pimatic API Established');
  $("#body-message").html("Connection to Pimatic API Established");	
});
//SHOW ALL EVENTS ON CONSOLE
socket.on('event', function(data) {
  console.log(data);
});
//MESSAGE ON CONSOLE WHEN DISCONNECTED
socket.on('disconnect', function(data) {
  console.log('Connection to Pimatic API lost, reconnecting...');
});

// When the connection is established, get all the Pimatic info and 'build' our GUI site
$( document ).ready(function() {
	GetStates();
});



//***********************************************
//**************API: DEVICES API*****************
//*********BEGIN ON-CONNECT STATE CHECKS*********
//***THESE FILL UP THE USER INTERFACE ON START***
//***********************************************
var gateState = {
    false : "Open",
    true : "Closed"
};
function GetStates()
{
	socket.on('devices', function(devices)
	{
		console.log(devices);
		
		// Loop through all the devices we get from Pimatic
		$.each( devices, function( key, value )
		{
			// Find every switch that's also in the html and set correct button state
			if(devices[key].template == "switch" && $("#gui").find("#" + devices[key].id).length > 0)
			{
				toggleUI(devices[key]["attributes"][0].value, devices[key].id)
			}

			// Find every presence device that's also in the html and set correct state
			if((devices[key].template == "presence" || devices[key].template == "contact") && $("#gui").find("#" + devices[key].id).length > 0)
			{
				toggleCircle(devices[key].attributes[0].value, devices[key].id)
			}

			if (devices[key].id == "salon")
			{				
				$("#gui").find('#LivingRoomTemperature').html(devices[key].attributes[0].value.toFixed(1))
			}
			if (devices[key].id == "exterieur")
			{
				$("#gui").find('#OutsideTemperature').html(devices[key].attributes[0].value.toFixed(1));
			}

			if (devices[key].id == "pression")
			{				
				$("#gui").find('#PowerValue').html(devices[key].attributes[0].value +" hpa")
			}
			if (devices[key].id == "temp-piscine")
			{				
				$("#gui").find('#PiscineTemp').html(devices[key].attributes[0].value.toFixed(1) +" °C")
			}			
			if (devices[key].id == "status-portail4")
			{				
				$("#gui").find('#GateStatus').html(gateState[devices[key].attributes[0].value])
				$("#gui").find('#GateStatus').css("color", "#a9a9a9");
				if (devices[key].attributes[0].value == false)
				{ 
					$("#gui").find('#GateStatus').css("color", "red");
				}
			}
            if (devices[key].id == "pompe-piscine")
            {
	            $("#gui").find('#Piscine').css("color", "#a9a9a9");
	            if (devices[key].attributes[0].value == true)
	            { 
		            $("#gui").find('#Piscine').css("color", "red");
	            }
            }   
			});


		
		// Now that we have all the info, and set all the buttons/presence/text we can show the GUI
		$("#loading").hide();
		$("#gui").show();
	});
}
   
//***********************************************
//***********REALTIME SOCKET CHECK***************
//**** THESE TAKE CARE THAT THE FIELDS ARE ******
//*************** ALWAYS UP TO DATE *************
//***********************************************

socket.on('deviceAttributeChanged', function(attrEvent) {
	console.log(attrEvent);

   if (attrEvent.deviceId == "salon")
   {
		$("#gui").find('#LivingRoomTemperature').html(attrEvent.value.toFixed(1));
   }

   if (attrEvent.deviceId == "exterieur")
   {
		$("#gui").find('#OutsideTemperature').html(attrEvent.value.toFixed(1));
   }
   if (attrEvent.deviceId == "pression")
   {
		$("#gui").find('#PowerValue').html(attrEvent.value +" hpa");
   }
   if (attrEvent.deviceId == "temp-piscine")
   {
		$("#gui").find('#PiscineTemp').html(attrEvent.value.toFixed(1) +" °C");
   }

   if (attrEvent.deviceId == "status-portail4")
   {
		$("#gui").find('#GateStatus').html(gateState[attrEvent.value]);
		$("#gui").find('#GateStatus').css("color", "#a9a9a9");
		if (attrEvent.value == false)
		{ 
			$("#gui").find('#GateStatus').css("color", "red");
		}
   }   
   if (attrEvent.deviceId == "pompe-piscine")
   {
		$("#gui").find('#Piscine').css("color", "#a9a9a9");
		if (attrEvent.value == true)
		{ 
			$("#gui").find('#Piscine').css("color", "red");
		}
   }   

   if (attrEvent.deviceId == "thermostaat" && attrEvent.attributeName == "temperatureSetpoint")
   {
	   $("#gui").find('#setpoint').html("Setpoint " + attrEvent.value +" °C");
   }

   if (attrEvent.deviceId == "thermostaat" && attrEvent.attributeName == "mode")
   {
	   $("#gui").find('#mode').html(" [" + attrEvent.value + "]");
   }
   
	// Find out if he update we got from Pimatic is also in our HTML
	if($("#gui").find("#" + attrEvent.deviceId).length > 0)
	{
		// If we have this device and our HTML class is 'uibutton' then treat it as a button (ON/OFF) device
		if ($("#gui").find("#" + attrEvent.deviceId).hasClass("uibutton"))
		{
			toggleUI(attrEvent.value, attrEvent.deviceId)
		}
		
		// If we have this device and our HTML class is 'circle' then treat it as a presence (absent/present) device
		if ($("#gui").find("#" + attrEvent.deviceId).hasClass("circle"))
		{
			toggleCircle(attrEvent.value, attrEvent.deviceId)
		}
	}
});
//***********************************************
//END OF SOCKET IO RECEIVING PARTS***************
//START OF SOCKET IO SENDING PARTS***************
//DIRTY SOLUTION TO BIND BUTTONS TO ACTIONS******
//***********************************************

function ChangeSetpoint(type){
	socket.emit('call', {
        id: 'executeAction-1',
        action: 'executeAction',
        params: {
            actionString: 'press ' + type
        }
    });

}

function toggleDevice(device){
	socket.emit('call', {
        id: 'executeAction-1',
        action: 'executeAction',
        params: {
            actionString: 'toggle ' + device
        }
    });
}

function toggleUI(state, device)
{
	if (state == true)
	{
		$("#gui").find('#' + device).addClass('active');
	}
	else
	{
		$("#gui").find('#' + device).removeClass('active');
	}
}

function toggleCircle(state, device)
{
	if (state == true)
	{
		$("#gui").find("#" + device).html("<span class='fa fa-circle pull-right'></span>");
	}			
	if (state == false)
	{
		$("#gui").find("#" + device).html("<span class='fa fa-circle-o pull-right'></span>");
	}
}

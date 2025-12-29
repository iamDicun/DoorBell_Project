import React from 'react';
import VoicemailList from '../../components/Voicemail/VoicemailList';

function VoicemailTab({ voicemails, isLoading }) {
  return <VoicemailList voicemails={voicemails} isLoading={isLoading} />;
}

export default VoicemailTab;
